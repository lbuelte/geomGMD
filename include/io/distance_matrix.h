#ifndef GEOMGMD_IO_DISTANCE_MATRIX_H
#define GEOMGMD_IO_DISTANCE_MATRIX_H

// Distances between the graphs graphs[0], ..., graphs[n-1] (local indices 0..n-1), computed in parallel or
// on demand. Every computed pair is reported like in all executables: Gurobi log (if wanted), error message with
// the graph ids if it fails, edge map and computation info row in the OutputContext if it succeeds.
// All member functions are thread-safe; a pair is computed at most once (a failed pair is not computed again).

#include "distance/geomGMD.h"
#include "io/config.h"
#include "io/output_context.h"
#include "utility/geometric_graph.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

class DistanceMatrix {
public:
    DistanceMatrix(std::vector<const GeometricGraph*> graphs, ModelParameters const &model,
                   SolverSettings const &settings, LoggingConfig const &logging, OutputContext &output);

    size_t size() const { return _graphs.size(); }

    // If set, pairs whose computation failed have the distance +infinity (get, known, to_matrix) instead of
    // none. They are still listed by failed_pairs().
    void treat_failed_as_infinite(bool infinite) { _failed_infinite = infinite; }
    bool failed_as_infinite() const { return _failed_infinite; }

    // Sets a known distance (e.g. from an earlier computation); it is not computed again.
    void set(size_t i, size_t j, IntervalDistance const &d);
    // The distance if it is known (computed or set), without computing it.
    std::optional<IntervalDistance> known(size_t i, size_t j) const;

    // The distance, computed if it is not known yet; nullopt if its computation failed.
    std::optional<IntervalDistance> get(size_t i, size_t j);
    // As get, but a computation stops as soon as the distance is known to be at least cutoff (Gurobi cutoff);
    // then nullopt is returned and the pair stays unknown (it can be computed later).
    std::optional<IntervalDistance> get_with_cutoff(size_t i, size_t j, double cutoff);

    // Computes all pairs i < j whose distance is not known yet, with num_threads threads (pairs are handed
    // out in batches of batch_size, in the order (0,1), (0,2), ..., (1,2), ...), and shows a progress bar.
    void compute_all(size_t num_threads, size_t batch_size);
    // The same for the pairs among the given graphs only (pairs (indices[a], indices[b]) for a < b).
    void compute_among(std::vector<size_t> const &indices, size_t num_threads, size_t batch_size);

    // Pairs (i < j) whose computation failed.
    std::vector<std::pair<size_t, size_t>> failed_pairs() const;
    // Graph id of graph i
    GraphID graph_id(size_t i) const { return static_cast<GraphID>(_graphs[i]->graph_id()); }

    // Matrix with the placeholder {-1, -1} for pairs without a distance; diagonal {0, 0}.
    std::vector<std::vector<IntervalDistance>> to_matrix() const;

private:
    enum class State { Unknown, Computing, Known, Failed };

    // Waits while (i, j) is being computed by another thread. Returns true if the caller should compute it
    // (it is then marked as Computing); otherwise *result is the known distance or nullopt (failed).
    bool claim(size_t i, size_t j, std::optional<IntervalDistance> *result);
    // Computes and reports a claimed pair (i, j), optionally with a Gurobi cutoff; the distance, or nullopt.
    std::optional<IntervalDistance> compute_pair(size_t i, size_t j, std::optional<double> cutoff = std::nullopt);
    void store(size_t i, size_t j, State state, std::optional<IntervalDistance> d);

    std::vector<const GeometricGraph*> _graphs;
    ModelParameters const _model;
    SolverSettings const _settings;
    LoggingConfig const _logging;
    OutputContext &_output;

    mutable std::mutex _mutex;  // guards _state, _distances and _failed
    std::condition_variable _computed;
    std::vector<std::vector<State>> _state;
    std::vector<std::vector<std::optional<IntervalDistance>>> _distances;
    std::vector<std::pair<size_t, size_t>> _failed;
    bool _failed_infinite = false;
};

// A subset of the graphs of a DistanceMatrix (e.g. a CLARA sample): index i of the view is index indices[i]
// of the matrix. Distances computed through the view are stored in the matrix (and reused by other views).
class DistanceView {
public:
    // All graphs of the matrix
    explicit DistanceView(DistanceMatrix &matrix);
    DistanceView(DistanceMatrix &matrix, std::vector<size_t> indices);

    size_t size() const { return _indices.size(); }
    // Index of view index i in the matrix
    size_t matrix_index(size_t i) const { return _indices[i]; }
    DistanceMatrix const &matrix() const { return *_matrix; }

    std::optional<IntervalDistance> known(size_t i, size_t j) const { return _matrix->known(_indices[i], _indices[j]); }
    std::optional<IntervalDistance> get(size_t i, size_t j) { return _matrix->get(_indices[i], _indices[j]); }
    void compute_all(size_t num_threads, size_t batch_size) { _matrix->compute_among(_indices, num_threads, batch_size); }
    // Failed pairs among the graphs of the view (view indices, i < j)
    std::vector<std::pair<size_t, size_t>> failed_pairs() const;
    // Matrix of the view with the placeholder {-1, -1} for pairs without a distance; diagonal {0, 0}.
    std::vector<std::vector<IntervalDistance>> to_matrix() const;

private:
    DistanceMatrix *_matrix;
    std::vector<size_t> _indices;
};

#endif // GEOMGMD_IO_DISTANCE_MATRIX_H
