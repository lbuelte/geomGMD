#ifndef POLYGONCLUSTERING_CLUSTERING_BASE_H
#define POLYGONCLUSTERING_CLUSTERING_BASE_H

#include "io/config.h"
#include "io/output_context.h"
#include "distance/geomGMDSolver.h"
#include "io/distance_matrix.h"

#include <thread>
#include <vector>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <string>

using ClusterID = size_t;

struct ClusteringResult {
    std::vector<std::vector<GraphID>> clusters = std::vector<std::vector<GraphID>>();
    std::vector<GraphID> medoid_indices = std::vector<GraphID>();
    double clustering_solution_value = 0;
    std::optional<std::vector<std::vector<IntervalDistance>>> _distance_matrix_opt = std::nullopt;
    // graphs left out because of failed distances ([Clustering] failed_distances = "exclude")
    std::vector<GraphID> excluded = std::vector<GraphID>();
};

// Thrown if distances between graphs to be clustered could not be computed; pairs() lists them (graph ids).
class FailedDistancesError : public std::runtime_error {
public:
    FailedDistancesError(std::string const &message, std::vector<std::pair<GraphID, GraphID>> pairs)
        : std::runtime_error(message), _pairs(std::move(pairs)) {}
    std::vector<std::pair<GraphID, GraphID>> const &pairs() const { return _pairs; }
private:
    std::vector<std::pair<GraphID, GraphID>> _pairs;
};

// Throws a FailedDistancesError if a distance between the graphs to be clustered could not be computed, unless
// such distances are treated as infinite (DistanceMatrix::failed_as_infinite). run_clustering then aborts or
// excludes graphs, depending on [Clustering] failed_distances.
void abort_on_failed_pairs(DistanceMatrix const &distances);
// The same for the pairs among the graphs of a view only.
void abort_on_failed_pairs(DistanceView const &distances);

ClusteringResult run_clustering(std::vector<const GeometricGraph*> const &graphs, size_t const k, Config const &config, OutputContext &out);

// Text representation of a clustering (clusters with medoids; graph ids shifted by starting_offset) for the log.
std::string clustering_result_to_string(
        ClusteringResult const &clustering_result,
        std::string const &clustering_name,
        size_t starting_offset = 0);

#endif // POLYGONCLUSTERING_CLUSTERING_BASE_H