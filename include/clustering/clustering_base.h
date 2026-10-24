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
#include <limits>
#include <optional>
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

// Building blocks of the clustering algorithms. distance(i, m) returns the distance between graphs i and m
// as std::optional<IntervalDistance> (none: unknown/failed, the medoid is skipped).

struct NearestMedoid {
    ClusterID index;            // position in medoids
    IntervalDistance distance;  // infinite if no distance to any medoid is known
};

// The medoid nearest to graph i: the first one with the smallest distance (IntervalDistance::operator<,
// i.e. by ub). If no medoid is nearer than infinity, the first medoid.
template<class Distance>
NearestMedoid nearest_medoid(size_t i, std::vector<GraphID> const &medoids, Distance &&distance)
{
    NearestMedoid nearest{0, {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()}};
    for (ClusterID m = 0; m < medoids.size(); ++m) {
        std::optional<IntervalDistance> const d = distance(i, medoids[m]);
        if (d.has_value() and d.value() < nearest.distance) {
            nearest = {m, d.value()};
        }
    }
    return nearest;
}

// Sum of the distances (ub) of the graphs 0, ..., n-1 to their nearest medoids
template<class Distance>
double total_cost(size_t n, std::vector<GraphID> const &medoids, Distance &&distance)
{
    double total = 0.0;
    for (size_t i = 0; i < n; ++i) {
        total += nearest_medoid(i, medoids, distance).distance.ub;
    }
    return total;
}

// Cluster (position in medoids) of each of the graphs 0, ..., n-1: its nearest medoid
template<class Distance>
std::vector<ClusterID> assign_to_medoids(size_t n, std::vector<GraphID> const &medoids, Distance &&distance)
{
    std::vector<ClusterID> assignment(n);
    for (size_t i = 0; i < n; ++i) {
        assignment[i] = nearest_medoid(i, medoids, distance).index;
    }
    return assignment;
}

ClusteringResult run_clustering(std::vector<const GeometricGraph*> const &graphs, size_t const k, Config const &config, OutputContext &out);

// Text representation of a clustering (clusters with medoids; graph ids shifted by starting_offset) for the log.
std::string clustering_result_to_string(
        ClusteringResult const &clustering_result,
        std::string const &clustering_name,
        size_t starting_offset = 0);

#endif // POLYGONCLUSTERING_CLUSTERING_BASE_H