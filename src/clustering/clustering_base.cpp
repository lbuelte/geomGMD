#include "clustering/clustering_base.h"

#include "clustering/clara.h"
#include "clustering/clarans.h"
#include "utility/log.h"

#include <cassert>
#include <sstream>
#include <queue>


void ClusteringResult::append_cluster_result(ClusteringResult const &other)
{
    assert(other.clusters.size() == other.medoid_indices.size());

    clustering_solution_value += other.clustering_solution_value;    
    for (ClusterID c = 0; c < other.clusters.size(); c++){
        clusters.push_back(other.clusters[c]);
        medoid_indices.push_back(other.medoid_indices[c]);
    }
}

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
            PAM_Clustering pam_clustering(graphs, config, out);
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