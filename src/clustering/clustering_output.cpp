#include "clustering/clustering_output.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace {

struct ClusteredGraph {
    GraphID graph_id;
    std::optional<PolygonID> polygon_id;  // none if the input file has no "id"
    ClusterID cluster;
    bool is_medoid;
};

// All clustered graphs with their global ids, sorted by graph id
std::vector<ClusteredGraph> clustered_graphs(ClusteringResult const &clustering_result,
                                             std::vector<const GeometricGraph*> const &graphs)
{
    std::vector<ClusteredGraph> result;
    for (ClusterID c = 0; c < clustering_result.clusters.size(); c++) {
        const GraphID medoid = clustering_result.medoid_indices[c];
        for (GraphID g : clustering_result.clusters[c]) {
            result.push_back({graphs[g]->graph_id(), graphs[g]->polygon_id(), c, g == medoid});
        }
    }
    std::sort(result.begin(), result.end(),
              [](auto const &a, auto const &b) { return a.graph_id < b.graph_id; });
    return result;
}

nlohmann::ordered_json ids_to_json(GeometricGraph const &graph)
{
    nlohmann::ordered_json j;
    j["graph_id"] = graph.graph_id();
    j["polygon_id"] = graph.polygon_id() ? nlohmann::ordered_json(*graph.polygon_id()) : nlohmann::ordered_json(nullptr);
    return j;
}

std::ofstream open_output(std::filesystem::path const &path)
{
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (not out.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }
    return out;
}

} // end of anonymous namespace


void write_clustering_csv(std::filesystem::path const &path,
                          ClusteringResult const &clustering_result,
                          std::vector<const GeometricGraph*> const &graphs)
{
    std::ofstream out = open_output(path);
    out << "graph_id,polygon_id,cluster,is_medoid\n";
    for (auto const &g : clustered_graphs(clustering_result, graphs)) {
        out << g.graph_id << ","
            << (g.polygon_id ? std::to_string(*g.polygon_id) : "") << ","
            << g.cluster << ","
            << (g.is_medoid ? "true" : "false") << "\n";
    }
    // graphs left out because of failed distances: no cluster
    for (GraphID g : clustering_result.excluded) {
        out << graphs[g]->graph_id() << ","
            << (graphs[g]->polygon_id() ? std::to_string(*graphs[g]->polygon_id()) : "") << ",,false\n";
    }
    if (not out.good()) {
        throw std::runtime_error("I/O error while writing: " + path.string());
    }
}

nlohmann::ordered_json clustering_to_json(ClusteringResult const &clustering_result,
                                  std::vector<const GeometricGraph*> const &graphs,
                                  std::string const &dataset,
                                  std::string const &run_name,
                                  std::string const &algorithm,
                                  size_t k)
{
    nlohmann::ordered_json j;
    j["dataset"] = dataset;
    j["run_name"] = run_name;
    j["algorithm"] = algorithm;
    j["k"] = k;
    j["solution_value"] = clustering_result.clustering_solution_value;
    j["clusters"] = nlohmann::ordered_json::array();
    for (ClusterID c = 0; c < clustering_result.clusters.size(); c++) {
        nlohmann::ordered_json cluster;
        cluster["cluster"] = c;
        cluster["medoid"] = ids_to_json(*graphs[clustering_result.medoid_indices[c]]);
        cluster["members"] = nlohmann::ordered_json::array();
        std::vector<GraphID> members = clustering_result.clusters[c];
        std::sort(members.begin(), members.end(),
                  [&](GraphID a, GraphID b) { return graphs[a]->graph_id() < graphs[b]->graph_id(); });
        for (GraphID g : members) {
            cluster["members"].push_back(ids_to_json(*graphs[g]));
        }
        j["clusters"].push_back(cluster);
    }
    if (not clustering_result.excluded.empty()) {
        j["excluded"] = nlohmann::ordered_json::array();
        for (GraphID g : clustering_result.excluded) {
            j["excluded"].push_back(ids_to_json(*graphs[g]));
        }
    }
    return j;
}

void write_clustering_json(std::filesystem::path const &path, nlohmann::ordered_json const &clustering)
{
    std::ofstream out = open_output(path);
    out << clustering.dump(2) << "\n";
    if (not out.good()) {
        throw std::runtime_error("I/O error while writing: " + path.string());
    }
}
