#include "io/distance_matrix.h"

#include "io/edgemap_io.h"
#include "io/solver_messages.h"
#include "utility/log.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <queue>
#include <thread>
#include <variant>

namespace {

void print_progress_bar(size_t current, size_t total, int bar_width = 50)
{
    double const progress = total > 0 ? static_cast<double>(current) / total : 1.0;
    int const pos = static_cast<int>(bar_width * progress);

    std::cout << "[";
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] "
              << std::fixed << std::setprecision(1)
              << (progress * 100.0) << "%  ("
              << current << " / " << total << ")\r";
    std::cout.flush();
}

} // end of anonymous namespace


DistanceMatrix::DistanceMatrix(std::vector<const GeometricGraph*> graphs, ModelParameters const &model,
                               SolverSettings const &settings, LoggingConfig const &logging, OutputContext &output)
    : _graphs(std::move(graphs))
    , _model(model)
    , _settings(settings)
    , _logging(logging)
    , _output(output)
    , _state(_graphs.size(), std::vector<State>(_graphs.size(), State::Unknown))
    , _distances(_graphs.size(), std::vector<std::optional<IntervalDistance>>(_graphs.size()))
{
    for (size_t i = 0; i < _graphs.size(); ++i) {
        _state[i][i] = State::Known;
        _distances[i][i] = IntervalDistance(ExactDistance{0.0});
    }
}

void DistanceMatrix::store(size_t i, size_t j, State state, std::optional<IntervalDistance> d)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _state[i][j] = _state[j][i] = state;
        _distances[i][j] = _distances[j][i] = d;
        if (state == State::Failed) _failed.emplace_back(std::min(i, j), std::max(i, j));
    }
    _computed.notify_all();
}

void DistanceMatrix::set(size_t i, size_t j, IntervalDistance const &d)
{
    store(i, j, State::Known, d);
}

namespace {
IntervalDistance infinite_distance()
{
    return {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
}
} // end of anonymous namespace

std::optional<IntervalDistance> DistanceMatrix::known(size_t i, size_t j) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_state[i][j] == State::Failed and _failed_infinite) return infinite_distance();
    return _state[i][j] == State::Known ? _distances[i][j] : std::nullopt;
}

bool DistanceMatrix::claim(size_t i, size_t j, std::optional<IntervalDistance> *result)
{
    std::unique_lock<std::mutex> lock(_mutex);
    _computed.wait(lock, [&]() { return _state[i][j] != State::Computing; });
    if (_state[i][j] == State::Unknown) {
        _state[i][j] = _state[j][i] = State::Computing;
        return true;
    }
    if (_state[i][j] == State::Failed and _failed_infinite) *result = infinite_distance();
    else *result = _state[i][j] == State::Known ? _distances[i][j] : std::nullopt;
    return false;
}

std::optional<IntervalDistance> DistanceMatrix::get(size_t i, size_t j)
{
    std::optional<IntervalDistance> result;
    return claim(i, j, &result) ? compute_pair(i, j) : result;
}

std::optional<IntervalDistance> DistanceMatrix::get_with_cutoff(size_t i, size_t j, double cutoff)
{
    std::optional<IntervalDistance> result;
    return claim(i, j, &result) ? compute_pair(i, j, cutoff) : result;
}

std::optional<IntervalDistance> DistanceMatrix::compute_pair(size_t i, size_t j, std::optional<double> cutoff)
{
    GeometricGraph const &G = *_graphs[i];
    GeometricGraph const &H = *_graphs[j];
    auto const r = compute_geomGMD(G, H, _model, _settings,
                                   {.cutoff = cutoff, .capture_gurobi_log = _logging.gurobi_log});
    report_gurobi_log(_logging, G, H, r);

    if (r.distance.has_value()) {
        _output.write_computation_info_row(r);
        _output.write_edgemap(edgemap_json(r));
        store(i, j, State::Known, r.distance);
        return r.distance;
    }
    if (std::holds_alternative<CutoffHit>(r.result)) {
        // Not an error: the distance is at least the cutoff; the pair stays unknown
        store(i, j, State::Unknown, std::nullopt);
        return std::nullopt;
    }
    report_solver_failure(r.result);
    geomgmd::log::error("Error! Distance between graphs ", G.graph_id(), " and ", H.graph_id(),
                        " could not be computed!");
    store(i, j, State::Failed, std::nullopt);
    if (_failed_infinite) return infinite_distance();
    return std::nullopt;
}

void DistanceMatrix::compute_all(size_t num_threads, size_t batch_size)
{
    std::vector<size_t> indices(size());
    for (size_t i = 0; i < size(); ++i) indices[i] = i;
    compute_among(indices, num_threads, batch_size);
}

void DistanceMatrix::compute_among(std::vector<size_t> const &indices, size_t num_threads, size_t batch_size)
{
    size_t const n = indices.size();
    size_t const total_pairs = n * (n - 1) / 2;

    // Batches of pairs whose distance is not known yet
    std::queue<std::vector<std::pair<size_t, size_t>>> tasks;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::pair<size_t, size_t>> current;
        current.reserve(batch_size);
        for (size_t a = 0; a < n; ++a) {
            for (size_t b = a + 1; b < n; ++b) {
                size_t const i = indices[a], j = indices[b];
                if (_state[i][j] != State::Unknown) continue;
                current.emplace_back(i, j);
                if (current.size() == batch_size) {
                    tasks.push(std::move(current));
                    current = {};
                    current.reserve(batch_size);
                }
            }
        }
        if (not current.empty()) tasks.push(std::move(current));
    }

    std::mutex queue_mutex, progress_mutex;
    size_t computed_pairs = 0;
    auto worker = [&]() {
        while (true) {
            std::vector<std::pair<size_t, size_t>> task;
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (tasks.empty()) break;
                task = std::move(tasks.front());
                tasks.pop();
            }
            for (auto const &[i, j] : task) {
                std::optional<IntervalDistance> d;
                if (claim(i, j, &d)) d = compute_pair(i, j);
                if (not d) continue;
                std::lock_guard<std::mutex> lock(progress_mutex);
                ++computed_pairs;
                if (computed_pairs % 5 == 0 or computed_pairs == total_pairs) {
                    print_progress_bar(computed_pairs, total_pairs);
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (size_t t = 0; t < std::max<size_t>(num_threads, 1); ++t) {
        threads.emplace_back(worker);
    }
    for (auto &t : threads) {
        t.join();
    }
    print_progress_bar(total_pairs, total_pairs);
    std::cout << std::endl;  // end the progress bar line
}

std::vector<std::pair<size_t, size_t>> DistanceMatrix::failed_pairs() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _failed;
}

std::vector<std::vector<IntervalDistance>> DistanceMatrix::to_matrix() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    size_t const n = _distances.size();  // _distances is only set for Known pairs
    std::vector<std::vector<IntervalDistance>> matrix(n, std::vector<IntervalDistance>(n, {-1.0, -1.0}));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (_distances[i][j]) matrix[i][j] = *_distances[i][j];
            else if (_state[i][j] == State::Failed and _failed_infinite) matrix[i][j] = infinite_distance();
        }
    }
    return matrix;
}


DistanceView::DistanceView(DistanceMatrix &matrix)
    : _matrix(&matrix)
    , _indices(matrix.size())
{
    for (size_t i = 0; i < _indices.size(); ++i) _indices[i] = i;
}

DistanceView::DistanceView(DistanceMatrix &matrix, std::vector<size_t> indices)
    : _matrix(&matrix)
    , _indices(std::move(indices))
{}

std::vector<std::pair<size_t, size_t>> DistanceView::failed_pairs() const
{
    std::vector<std::pair<size_t, size_t>> failed;
    for (auto const &[i, j] : _matrix->failed_pairs()) {
        auto const a = std::find(_indices.begin(), _indices.end(), i);
        auto const b = std::find(_indices.begin(), _indices.end(), j);
        if (a == _indices.end() or b == _indices.end()) continue;
        size_t const va = a - _indices.begin(), vb = b - _indices.begin();
        failed.emplace_back(std::min(va, vb), std::max(va, vb));
    }
    return failed;
}

std::vector<std::vector<IntervalDistance>> DistanceView::to_matrix() const
{
    size_t const n = size();
    std::vector<std::vector<IntervalDistance>> matrix(n, std::vector<IntervalDistance>(n, {-1.0, -1.0}));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (auto const d = known(i, j)) matrix[i][j] = *d;
        }
    }
    return matrix;
}
