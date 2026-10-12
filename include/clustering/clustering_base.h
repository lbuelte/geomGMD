#ifndef CLUSTERING_BASE_H
#define CLUSTERING_BASE_H

#include "va_graph.h"
#include "io/config.h"
#include "io/output_context.h"
#include "geomGMDSolver.h"

#include <thread>
#include <vector>
#include <cstddef>

using ClusterID = size_t;

struct ClusteringResult {
    std::vector<std::vector<GraphID>> clusters = std::vector<std::vector<GraphID>>();
    std::vector<GraphID> medoid_indices = std::vector<GraphID>();
    double clustering_solution_value = 0;
    std::optional<std::vector<std::vector<IntervalDistance>>> _distance_matrix_opt = std::nullopt;

    // void append_cluster_result_and_propagate_ids(ClusteringResult const &other, std::vector<GraphID> const &graph_ids);
    void append_cluster_result(ClusteringResult const &other);
};

ClusteringResult run_clustering_single_mode(std::vector<const GeometricGraph*> const &graphs, size_t const k, Config const &config, OutputContext &out);
ClusteringResult run_clustering_va_mode(std::vector<VAGraph> &graph_base, Config const &config, OutputContext &out);

struct ClusteringInput{
    std::vector<const GeometricGraph*> graphs;
    std::vector<GraphID> const graph_ids;
    double const vanishing_angle;
    size_t const k;
};

// recursive version of run_clustering_va_mode
// void compute_clustering(ClusteringResult &final_clustering, std::vector<VAGraph> &graph_base, std::vector<GraphID> const &graph_ids, double const vanishing_angle, Config const &config);

void read_symmetric_IntervalDistance_matrix_from_csv(std::vector<std::vector<IntervalDistance>> &matrix, const std::string& filename);

#endif // CLUSTERING_BASE_H