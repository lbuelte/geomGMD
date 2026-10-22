#include "clustering/clustering_base.h"

#include "clustering/clara.h"
#include "clustering/clarans.h"
#include "io/read_run_results.h"
#include "utility/log.h"

#include <filesystem>
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
        geomgmd::log::warning(run_config, " does not set [Gurobi] ", keys, ": assuming the current defaults");
    }
    if (not (file_model == model)) {
        std::ostringstream msg;
        msg << "The distances in " << matrix_file.string() << " were computed with other model parameters"
            << " (" << run_config.string() << ")\n" << file_model << "than this clustering\n" << model;
        throw std::runtime_error(msg.str());
    }
}

// Distance matrix of the graphs to be clustered, filled from [Clustering] distance_matrix (graph ids in the
// file, local indices in the matrix). Pairs missing in the file keep the placeholder {-1, -1} and are
// computed by PAM, unless compute_missing_distances is false (then this throws).
std::vector<std::vector<IntervalDistance>> read_precomputed_distances(
        std::vector<const GeometricGraph*> const &graphs, Config const &config)
{
    std::filesystem::path const matrix_file = config.cluster.distance_matrix.value();
    check_matrix_parameters(matrix_file, config.model);
    auto const distances = read_distance_matrix_file(matrix_file);

    size_t const n = graphs.size();
    std::vector<std::vector<IntervalDistance>> matrix(n, std::vector<IntervalDistance>(n, {-1.0, -1.0}));
    size_t num_read = 0, num_missing = 0;
    for (size_t i = 0; i < n; ++i) {
        matrix[i][i] = ExactDistance{0.0};
        for (size_t j = i + 1; j < n; ++j) {
            auto const d = distances.find(graph_pair(static_cast<GraphID>(graphs[i]->graph_id()),
                                                     static_cast<GraphID>(graphs[j]->graph_id())));
            if (d == distances.end()) {
                ++num_missing;
                continue;
            }
            matrix[i][j] = matrix[j][i] = d->second;
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
    return matrix;
}

} // end of anonymous namespace


ClusteringResult run_clustering(std::vector<const GeometricGraph*> const &graphs, size_t const k, 
        Config const &config, OutputContext &out)
{
    ClusteringResult clustering_result;
    switch (config.cluster.clustering_algo) {

        case ClusteringConfig::ClusteringAlgorithm::CLARA: {
            CLARA_Clustering clara_clustering(graphs, config, out);
            geomgmd::log::debug("Running CLARA Clustering (k=", k, ") on ", graphs.size(), " graphs on ",
                                config.pam.num_threads_matrix, "/", config.clara.num_threads_assignment,
                                " threads and ", config.clara.num_samples, " samples.");
            clustering_result = clara_clustering.cluster(k, config);
            if (config.io.write_distance_matrix){
                clustering_result._distance_matrix_opt = clara_clustering.return_distance_matrix();
            }
            break;
        }

        case ClusteringConfig::ClusteringAlgorithm::CLARANS: {
            CLARANS_Clustering clarans_clustering(graphs, config, out);
            geomgmd::log::debug("Running CLARANS Clustering (k=", k, ") on ", graphs.size(), " graphs on ",
                                config.pam.num_threads_matrix, " threads and ", config.clarans.num_local,
                                " starting samples.");
            clustering_result =  clarans_clustering.cluster(k, config.clarans);
            if (config.io.write_distance_matrix){
                clustering_result._distance_matrix_opt = clarans_clustering.return_distance_matrix();
            }
            break;
        }

        case ClusteringConfig::ClusteringAlgorithm::PAM: {
            auto pam_clustering = config.cluster.distance_matrix
                    ? [&]() { auto matrix = read_precomputed_distances(graphs, config);
                              return PAM_Clustering(graphs, matrix, config, out); }()
                    : PAM_Clustering(graphs, config, out);
            geomgmd::log::debug("Running PAM Clustering (k=", k, ") on ", graphs.size(), " graphs on ",
                                config.pam.num_threads_matrix, " threads.");
            clustering_result =  pam_clustering.cluster(config, k);
            if (config.io.write_distance_matrix){
                clustering_result._distance_matrix_opt = pam_clustering.return_distance_matrix();
            }
            break;
        }
        default:
            throw std::logic_error("Unhandled clustering algorithm");
    }
    return clustering_result;
}


void read_symmetric_IntervalDistance_matrix_from_csv(
        std::vector<std::vector<IntervalDistance>> &matrix, 
        const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    geomgmd::log::info("Reading IntervalDistance matrix from file ", filename, " ...");
    int n = matrix.size();

    std::string line;

    // Skip header row
    std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        int id1, id2;
        double lb, ub;
        std::string exactStr;
        char comma;

        if (!(iss >> id1 >> comma >> id2 >> comma >> lb >> comma >> ub >> comma >> exactStr)) {
            throw std::runtime_error("In read_IntervalDistance_matrix: Malformed line: " + line);
        }
        if (id1 < 0 or id1 >= n){
            throw std::runtime_error("In read_IntervalDistance_matrix: id1 " + std::to_string(id1) 
                                     + " is out of bounds for matrix of size " + std::to_string(n));
        }
        if (id2 < 0 or id2 >= n){
            throw std::runtime_error("In read_IntervalDistance_matrix: id2 " + std::to_string(id2) 
                                     + " is out of bounds for matrix of size " + std::to_string(n));
        }

        bool exact = (exactStr == "true");
        matrix[id1][id2] = exact
            ? IntervalDistance(ExactDistance{lb})
            : IntervalDistance(lb, ub);
        matrix[id2][id1] = exact
            ? IntervalDistance(ExactDistance{lb})
            : IntervalDistance(lb, ub);
    }
}

std::string clustering_result_to_string(ClusteringResult const &clustering_result, std::string const &clustering_name, size_t starting_offset)
{
    std::ostringstream log;
    log << clustering_name << " with solution value " << clustering_result.clustering_solution_value << " :";
    for (int c = 0; c < clustering_result.clusters.size(); c++){
        log << "\nCluster " << c << " with medoid " << clustering_result.medoid_indices[c] + starting_offset << " :\n";
        for (auto const & g : clustering_result.clusters[c]){
            log << g + starting_offset << " ";
        }
    }
    return log.str();
}