#ifndef POLYGONCLUSTERING_CLUSTERING_OUTPUT_H
#define POLYGONCLUSTERING_CLUSTERING_OUTPUT_H

#include "clustering/clustering_base.h"

#include "json/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

// Result files of a clustering. Both contain only the clustered graphs, identified by their graph id
// (position in the input file) and their polygon id ("id" in the input file; null/empty if missing).
// The cluster members and medoids in clustering_result are local indices into graphs.

// clustering.csv: one row per clustered graph, sorted by graph id:
//   graph_id,polygon_id,cluster,is_medoid
// followed by the excluded graphs (failed_distances = "exclude") with an empty cluster.
void write_clustering_csv(std::filesystem::path const &path,
                          ClusteringResult const &clustering_result,
                          std::vector<const GeometricGraph*> const &graphs);

// clustering.json: {"dataset", "run_name", "algorithm", "k", "solution_value",
//                   "clusters": [{"cluster", "medoid": {graph_id, polygon_id}, "members": [{graph_id, polygon_id}, ...]}],
//                   "excluded": [{graph_id, polygon_id}, ...] (only if graphs were excluded)}
nlohmann::ordered_json clustering_to_json(ClusteringResult const &clustering_result,
                                  std::vector<const GeometricGraph*> const &graphs,
                                  std::string const &dataset,
                                  std::string const &run_name,
                                  std::string const &algorithm,
                                  size_t k);

void write_clustering_json(std::filesystem::path const &path, nlohmann::ordered_json const &clustering);

#endif // POLYGONCLUSTERING_CLUSTERING_OUTPUT_H
