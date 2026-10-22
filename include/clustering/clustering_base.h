#ifndef POLYGONCLUSTERING_CLUSTERING_BASE_H
#define POLYGONCLUSTERING_CLUSTERING_BASE_H

#include "io/config.h"
#include "io/output_context.h"
#include "distance/geomGMDSolver.h"

#include <thread>
#include <vector>
#include <cstddef>
#include <fstream>
#include <string>

using ClusterID = size_t;

struct ClusteringResult {
    std::vector<std::vector<GraphID>> clusters = std::vector<std::vector<GraphID>>();
    std::vector<GraphID> medoid_indices = std::vector<GraphID>();
    double clustering_solution_value = 0;
    std::optional<std::vector<std::vector<IntervalDistance>>> _distance_matrix_opt = std::nullopt;
};

ClusteringResult run_clustering(std::vector<const GeometricGraph*> const &graphs, size_t const k, Config const &config, OutputContext &out);

// Text representation of a clustering (clusters with medoids; graph ids shifted by starting_offset) for the log.
std::string clustering_result_to_string(
        ClusteringResult const &clustering_result,
        std::string const &clustering_name,
        size_t starting_offset = 0);

#endif // POLYGONCLUSTERING_CLUSTERING_BASE_H