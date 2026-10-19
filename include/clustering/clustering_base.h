#ifndef POLYGONCLUSTERING_BASE_H
#define POLYGONCLUSTERING_BASE_H

#include "io/config.h"
#include "io/output_context.h"
#include "geomGMDSolver.h"

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

    // void append_cluster_result_and_propagate_ids(ClusteringResult const &other, std::vector<GraphID> const &graph_ids);
    void append_cluster_result(ClusteringResult const &other);
};

ClusteringResult run_clustering(std::vector<const GeometricGraph*> const &graphs, size_t const k, Config const &config, OutputContext &out);

void read_symmetric_IntervalDistance_matrix_from_csv(std::vector<std::vector<IntervalDistance>> &matrix, const std::string& filename);

// Text representation of a clustering (clusters with medoids; graph ids shifted by starting_offset) for the log.
std::string clustering_result_to_string(
        ClusteringResult const &clustering_result,
        std::string const &clustering_name,
        size_t starting_offset = 0);

#endif // POLYGONCLUSTERING_BASE_H