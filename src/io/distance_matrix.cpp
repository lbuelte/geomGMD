#include "io/distance_matrix.h"

#include "io/edgemap_io.h"
#include "io/solver_messages.h"
#include "utility/log.h"

#include <iomanip>
#include <iostream>
#include <queue>
#include <thread>

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
    , _distances(_graphs.size(), std::vector<std::optional<IntervalDistance>>(_graphs.size()))
{
    for (size_t i = 0; i < _graphs.size(); ++i) {
        _distances[i][i] = IntervalDistance(ExactDistance{0.0});
    }
}

void DistanceMatrix::set(size_t i, size_t j, IntervalDistance const &d)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _distances[i][j] = d;
    _distances[j][i] = d;
}

std::optional<IntervalDistance> DistanceMatrix::known(size_t i, size_t j) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _distances[i][j];
}

std::optional<IntervalDistance> DistanceMatrix::compute_pair(size_t i, size_t j)
{
    GeometricGraph const &G = *_graphs[i];
    GeometricGraph const &H = *_graphs[j];
    auto const r = compute_geomGMD(G, H, _model, _settings, {.capture_gurobi_log = _logging.gurobi_log});
    report_gurobi_log(_logging, G, H, r);

    if (not r.distance.has_value()) {
        report_solver_failure(r.result);
        geomgmd::log::error("Error! Distance between graphs ", G.graph_id(), " and ", H.graph_id(),
                            " could not be computed!");
        std::lock_guard<std::mutex> lock(_mutex);
        _failed.emplace_back(std::min(i, j), std::max(i, j));
        return std::nullopt;
    }
    _output.write_computation_info_row(r);
    _output.write_edgemap(edgemap_json(r));

    std::lock_guard<std::mutex> lock(_mutex);
    _distances[i][j] = r.distance;
    _distances[j][i] = r.distance;
    return r.distance;
}

void DistanceMatrix::compute_all(size_t num_threads, size_t batch_size)
{
    size_t const n = size();
    size_t const total_pairs = n * (n - 1) / 2;

    // Batches of pairs whose distance is not known yet
    std::queue<std::vector<std::pair<size_t, size_t>>> tasks;
    {
        std::vector<std::pair<size_t, size_t>> current;
        current.reserve(batch_size);
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                if (known(i, j).has_value()) continue;
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
                if (not compute_pair(i, j)) continue;
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
    size_t const n = _distances.size();
    std::vector<std::vector<IntervalDistance>> matrix(n, std::vector<IntervalDistance>(n, {-1.0, -1.0}));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (_distances[i][j]) matrix[i][j] = *_distances[i][j];
        }
    }
    return matrix;
}
