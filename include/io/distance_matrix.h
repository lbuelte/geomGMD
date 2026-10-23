#ifndef GEOMGMD_IO_DISTANCE_MATRIX_H
#define GEOMGMD_IO_DISTANCE_MATRIX_H

// Distances between the graphs graphs[0], ..., graphs[n-1] (local indices 0..n-1), computed in parallel.
// Every computed pair is reported like in all executables: Gurobi log (if wanted), error message with the
// graph ids if it fails, edge map and computation info row in the OutputContext if it succeeds.

#include "distance/geomGMD.h"
#include "io/config.h"
#include "io/output_context.h"
#include "utility/geometric_graph.h"

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

    // Sets a known distance (e.g. from an earlier computation); it is not computed again.
    void set(size_t i, size_t j, IntervalDistance const &d);
    // The distance if it is known (computed or set), without computing it.
    std::optional<IntervalDistance> known(size_t i, size_t j) const;

    // Computes all pairs i < j whose distance is not known yet, with num_threads threads (pairs are handed
    // out in batches of batch_size, in the order (0,1), (0,2), ..., (1,2), ...), and shows a progress bar.
    void compute_all(size_t num_threads, size_t batch_size);

    // Pairs (i < j) whose computation failed.
    std::vector<std::pair<size_t, size_t>> failed_pairs() const;

    // Matrix with the placeholder {-1, -1} for pairs without a distance; diagonal {0, 0}.
    std::vector<std::vector<IntervalDistance>> to_matrix() const;

private:
    // Computes and reports the pair (i, j); the distance, or nullopt if it failed. Thread-safe.
    std::optional<IntervalDistance> compute_pair(size_t i, size_t j);

    std::vector<const GeometricGraph*> _graphs;
    ModelParameters const _model;
    SolverSettings const _settings;
    LoggingConfig const _logging;
    OutputContext &_output;

    mutable std::mutex _mutex;  // guards _distances and _failed
    std::vector<std::vector<std::optional<IntervalDistance>>> _distances;
    std::vector<std::pair<size_t, size_t>> _failed;
};

#endif // GEOMGMD_IO_DISTANCE_MATRIX_H
