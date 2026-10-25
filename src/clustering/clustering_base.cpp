#include "clustering/clustering_base.h"

#include "clustering/clara.h"
#include "clustering/clarans.h"
#include "io/read_run_results.h"
#include "utility/log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <queue>

namespace {

// Throws if the model parameters of the run that computed the matrix file (its config_input.toml) differ
// from the clustering's; distances computed with other parameters must not be mixed.
void check_matrix_parameters(std::filesystem::path const &matrix_file, ModelParameters const &model)
{
    auto const run_config = matrix_file.parent_path() / "config_input.toml";
    if (not std::filesystem::exists(run_config)) {
        geomgmd::log::warning("No config_input.toml next to ", matrix_file,
                              ": cannot check that its distances were computed with the same model parameters");
        return;
    }
    auto const [file_model, missing_keys] = load_model_parameters(run_config.string());
    if (not missing_keys.empty()) {
        std::string keys;
        for (auto const &key : missing_keys) keys += (keys.empty() ? "" : ", ") + key;
        geomgmd::log::warning(run_config, " does not set [Gurobi] ", keys,
                              ": assuming the current default (cost_factor_angle_difference: 1.0, but 2.0 in the sigspatial26 version)");
    }
    if (not (file_model == model)) {
        std::ostringstream msg;
        msg << "The distances in " << matrix_file.string() << " were computed with other model parameters"
            << " (" << run_config.string() << ")\n" << file_model << "than this clustering\n" << model;
        throw std::runtime_error(msg.str());
    }
}

// Fills the distances of the graphs to be clustered from [Clustering] distance_matrix (graph ids in the
// file, local indices in the matrix). Pairs missing in the file stay unknown and are computed by PAM,
// unless compute_missing_distances is false (then this throws).
void read_precomputed_distances(DistanceMatrix &matrix, std::vector<const GeometricGraph*> const &graphs,
                                Config const &config)
{
    std::filesystem::path const matrix_file = config.cluster.distance_matrix.value();
    check_matrix_parameters(matrix_file, config.model);
    auto const distances = read_distance_matrix_file(matrix_file);

    size_t const n = graphs.size();
    size_t num_read = 0, num_missing = 0;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            auto const d = distances.find(graph_pair(graphs[i]->graph_id(), graphs[j]->graph_id()));
            if (d == distances.end()) {
                ++num_missing;
                continue;
            }
            matrix.set(i, j, d->second);
            ++num_read;
        }
    }
    geomgmd::log::info("Read ", num_read, " distances from ", matrix_file, "; ", num_missing, " pairs are missing",
                       num_missing > 0 and config.cluster.compute_missing_distances ? " and are computed" : "");
    if (num_missing > 0 and not config.cluster.compute_missing_distances) {
        throw std::runtime_error(std::to_string(num_missing) + " distances of the clustered graphs are missing in "
                                 + matrix_file.string() + " (set [Clustering] compute_missing_distances = true"
                                 " to compute them)");
    }
    if (config.io.write_edgemaps and num_read > 0) {
        geomgmd::log::warning("edgemaps.json of this run only contains the edge maps of distances computed in this run;",
                              " for the ", num_read, " distances read from ", matrix_file,
                              " see edgemaps.json in that folder");
    }
}

} // end of anonymous namespace


namespace {

void throw_failed_pairs(std::vector<std::pair<GraphID, GraphID>> graph_pairs)
{
    if (graph_pairs.empty()) return;
    // sorted, so that the message does not depend on the order in which threads computed the pairs
    for (auto &[g, h] : graph_pairs) {
        if (h < g) std::swap(g, h);
    }
    std::sort(graph_pairs.begin(), graph_pairs.end());
    auto const &failed = graph_pairs;
    std::ostringstream msg;
    msg << failed.size() << " distance(s) between the graphs to be clustered could not be computed (graph pairs";
    size_t const shown = std::min<size_t>(failed.size(), 20);
    for (size_t p = 0; p < shown; ++p) {
        msg << (p == 0 ? " " : ", ") << "(" << failed[p].first << ", " << failed[p].second << ")";
    }
    msg << (failed.size() > shown ? ", ..." : "") << "; see the errors above). The clustering is aborted.";
    throw FailedDistancesError(msg.str(), std::move(graph_pairs));
}

} // end of anonymous namespace

void abort_on_failed_pairs(DistanceMatrix const &distances)
{
    if (distances.failed_as_infinite()) return;
    std::vector<std::pair<GraphID, GraphID>> graph_pairs;
    for (auto const &[i, j] : distances.failed_pairs()) {
        graph_pairs.emplace_back(distances.graph_id(i), distances.graph_id(j));
    }
    throw_failed_pairs(std::move(graph_pairs));
}

void abort_on_failed_pairs(DistanceView const &distances)
{
    if (distances.matrix().failed_as_infinite()) return;
    std::vector<std::pair<GraphID, GraphID>> graph_pairs;
    for (auto const &[i, j] : distances.failed_pairs()) {
        graph_pairs.emplace_back(distances.matrix().graph_id(distances.matrix_index(i)),
                                 distances.matrix().graph_id(distances.matrix_index(j)));
    }
    throw_failed_pairs(std::move(graph_pairs));
}


namespace {

// As few graphs as possible such that every failed pair contains one of them (greedy: repeatedly the graph
// with the most failed pairs, ties by smaller graph id)
std::vector<GraphID> graphs_to_exclude(std::vector<std::pair<GraphID, GraphID>> pairs)
{
    std::vector<GraphID> excluded;
    while (not pairs.empty()) {
        std::map<GraphID, size_t> count;
        for (auto const &[g, h] : pairs) {
            ++count[g];
            ++count[h];
        }
        auto const most = std::max_element(count.begin(), count.end(),
                                           [](auto const &a, auto const &b) { return a.second < b.second; });
        GraphID const g = most->first;
        excluded.push_back(g);
        std::erase_if(pairs, [g](auto const &p) { return p.first == g or p.second == g; });
    }
    return excluded;
}

// One clustering of the graphs of the view (the result refers to positions in the view)
ClusteringResult cluster_once(DistanceView distances, size_t const k, Config const &config)
{
    switch (config.cluster.clustering_algo) {
        case ClusteringConfig::ClusteringAlgorithm::CLARA:
            geomgmd::log::debug("Running CLARA Clustering (k=", k, ") on ", distances.size(), " graphs on ",
                                config.pam.num_threads_matrix, "/", config.clara.num_threads_assignment,
                                " threads and ", config.clara.num_samples, " samples.");
            return cluster_clara(std::move(distances), k, config);
        case ClusteringConfig::ClusteringAlgorithm::CLARANS:
            geomgmd::log::debug("Running CLARANS Clustering (k=", k, ") on ", distances.size(), " graphs on ",
                                config.clarans.num_threads, " threads and ", config.clarans.num_local,
                                " starting samples.");
            return cluster_clarans(std::move(distances), k, config);
        case ClusteringConfig::ClusteringAlgorithm::PAM:
            geomgmd::log::debug("Running PAM Clustering (k=", k, ") on ", distances.size(), " graphs on ",
                                config.pam.num_threads_matrix, " threads.");
            return cluster_pam(std::move(distances), k, config);
    }
    throw std::logic_error("Unhandled clustering algorithm");
}

} // end of anonymous namespace


ClusteringResult run_clustering(std::vector<const GeometricGraph*> const &graphs, size_t const k,
        Config const &config, OutputContext &out)
{
    using FailedDistances = ClusteringConfig::FailedDistances;
    auto const mode = config.cluster.failed_distances;
    if (graphs.size() < k) {
        throw std::runtime_error("[Clustering] k = " + std::to_string(k) + " is larger than the number of graphs to be"
                                 " clustered (" + std::to_string(graphs.size()) + ")");
    }

    // Distances between all graphs, shared by the algorithm's steps and kept when graphs are excluded and the
    // clustering is repeated
    DistanceMatrix distances(graphs, config.model, config.solver, config.logging, out);
    distances.treat_failed_as_infinite(mode == FailedDistances::Infinite);
    if (config.cluster.distance_matrix) {
        read_precomputed_distances(distances, graphs, config);
    }

    std::vector<size_t> active(graphs.size());
    for (size_t p = 0; p < active.size(); ++p) active[p] = p;
    std::vector<size_t> excluded;

    ClusteringResult clustering_result;
    while (true) {
        try {
            clustering_result = cluster_once(DistanceView(distances, active), k, config);
            break;
        } catch (FailedDistancesError const &e) {
            if (mode != FailedDistances::Exclude) throw;
            // Exclude graphs so that no failed pair remains, and cluster the remaining ones again
            std::vector<GraphID> const ids = graphs_to_exclude(e.pairs());
            std::ostringstream text;
            for (GraphID id : ids) text << id << " ";
            geomgmd::log::warning("Excluding ", ids.size(), " graph(s) with failed distances from the clustering: ",
                                  text.str());
            size_t const num_active = active.size();
            std::erase_if(active, [&](size_t p) {
                bool const remove = std::find(ids.begin(), ids.end(), graphs[p]->graph_id())
                                    != ids.end();
                if (remove) excluded.push_back(p);
                return remove;
            });
            if (active.size() == num_active) {
                throw std::logic_error("Excluding graphs with failed distances removed no graph");
            }
            if (active.size() < k) {
                throw std::runtime_error("Only " + std::to_string(active.size()) + " graphs are left after excluding"
                                         " graphs with failed distances, fewer than k = " + std::to_string(k));
            }
        }
    }

    // Positions in active -> positions in graphs
    for (auto &cluster : clustering_result.clusters) {
        for (auto &g : cluster) g = active[g];
    }
    for (auto &m : clustering_result.medoid_indices) m = active[m];
    std::sort(excluded.begin(), excluded.end());
    clustering_result.excluded = excluded;
    if (config.io.write_distance_matrix) {
        clustering_result._distance_matrix_opt = distances.to_matrix();
    }

    if (mode == FailedDistances::Infinite and std::isinf(clustering_result.clustering_solution_value)) {
        geomgmd::log::warning("The solution value is infinite: some graph has failed distances to all medoids,"
                              " so this clustering is not meaningful (consider failed_distances = \"exclude\")");
    }
    return clustering_result;
}


std::string clustering_result_to_string(ClusteringResult const &clustering_result, std::string const &clustering_name, size_t starting_offset)
{
    std::ostringstream log;
    log << clustering_name << " with solution value " << clustering_result.clustering_solution_value << " :";
    for (ClusterID c = 0; c < clustering_result.clusters.size(); c++){
        log << "\nCluster " << c << " with medoid " << clustering_result.medoid_indices[c] + starting_offset << " :\n";
        for (auto const & g : clustering_result.clusters[c]){
            log << g + starting_offset << " ";
        }
    }
    if (not clustering_result.excluded.empty()) {
        log << "\nExcluded (failed distances):\n";
        for (auto const &g : clustering_result.excluded) log << g + starting_offset << " ";
    }
    return log.str();
}