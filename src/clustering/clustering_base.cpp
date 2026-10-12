#include "clustering/clustering_base.h"

#include "clustering/clara.h"
#include "clustering/clarans.h"
#include "io/logger.h"

#include <cassert>
#include <queue>


namespace{


struct IntermediateClustering{
    ClusteringResult const clustering_result;
    std::vector<size_t> const new_ks;
};


size_t estimate_new_k_for_cluster(std::vector<GraphID> const &cluster, std::vector<IntervalDistance> const &distance_to_medoid, double avg_dist)
{
    size_t new_k = 1;
    const size_t n = cluster.size();

    std::vector<double> sorted_distances;

    // First compute mean distance to medoid
    double total = 0;
    for (auto const &g : cluster){
        assert(distance_to_medoid[g].ub > -1); // TODO: need actual graph ids here!!!
        total += distance_to_medoid[g].ub;
        sorted_distances.push_back(distance_to_medoid[g].ub);
    }
    const double mean = total / static_cast<double>(n);

    // Now sort distances and compute median etc.
    std::sort(sorted_distances.begin(), sorted_distances.end());
    const double median = sorted_distances[n/2];

    const double ninetynine = sorted_distances[std::floor(0.99 * static_cast<double>(n))];
    
    if (mean > 2 * avg_dist) {
        global_log << "Increasing k due to mean = " << mean << " > 2*avg = " << 2 * avg_dist << std::endl;
        new_k++;
    }
    if (mean > 2 * median){ 
        global_log << "Increasing k due to mean = " << mean << " > 2*median = " << 2 * median << std::endl;
        new_k++;
    }
    if (ninetynine > 2 * mean) {
        global_log << "Increasing k due to ninetynine = " << ninetynine << " > 2*mean = " << 2 * mean << std::endl;
        new_k++;
    }
    return new_k;
}


std::vector<size_t> estimate_new_ks(ClusteringResult const &clustering)
{
    std::vector<size_t> res;
    const double avg_dist = clustering.clustering_solution_value / static_cast<double>(clustering._distance_matrix_opt.value().size());
    for (ClusterID c = 0; c < clustering.clusters.size(); c++){
        global_log << "Estimating new k for cluster " << c << std::endl;
        res.push_back( estimate_new_k_for_cluster(
                clustering.clusters[c], 
                clustering._distance_matrix_opt.value()[clustering.medoid_indices[c]],
                avg_dist));
    }
    return res;
}


// Returns CLARA clustering result with correct (corresponding to input) graph ids.
IntermediateClustering compute_intermediate_clustering(ClusteringInput const &input, Config const &config, OutputContext &out)
{
    std::cout << "Running single mode clustering with VA=" << input.vanishing_angle << ":" << std::endl;
    global_log << "Running single mode clustering with VA=" << input.vanishing_angle << ":" << std::endl;
    auto clustering_result = run_clustering_single_mode(input.graphs, input.k, config, out);

    std::vector<size_t> new_ks = 
        config.cluster.va_mode.value().fixed_k 
            ? std::vector<size_t>(clustering_result.clusters.size(), config.cluster.k)
            : estimate_new_ks(clustering_result);

    for (ClusterID c = 0; c < clustering_result.clusters.size(); c++){
        std::vector<GraphID> fine_graph_ids;
        for (GraphID i : clustering_result.clusters[c]){
            fine_graph_ids.push_back(input.graph_ids[i]);
        }
        clustering_result.clusters[c] = fine_graph_ids;
        clustering_result.medoid_indices[c] = input.graph_ids[clustering_result.medoid_indices[c]];
    }

    log_clustering_result(global_log, clustering_result, std::string("Intermediate clustering"));
    return {clustering_result, new_ks};
}

} // end of anonymous namespace


void ClusteringResult::append_cluster_result(ClusteringResult const &other)
{
    assert(other.clusters.size() == other.medoid_indices.size());

    clustering_solution_value += other.clustering_solution_value;    
    for (ClusterID c = 0; c < other.clusters.size(); c++){
        clusters.push_back(other.clusters[c]);
        medoid_indices.push_back(other.medoid_indices[c]);
    }
}

ClusteringResult run_clustering_single_mode(std::vector<const GeometricGraph*> const &graphs, size_t const k, 
        Config const &config, OutputContext &out)
{
    ClusteringResult clustering_result;
    switch (config.cluster.clustering_algo) {

        case ClusteringConfig::ClusteringAlgorithm::CLARA: {
            CLARA_Clustering clara_clustering(graphs, config, out);
            global_log << "\nRunning CLARA Clustering (k=" << k
                       << ") on " << graphs.size() << " graphs on " << config.pam.num_threads_matrix 
                       << "/" << config.clara.num_threads_assignment 
                       << " threads and " << config.clara.num_samples << " samples." << std::endl;
            clustering_result = clara_clustering.cluster(k, config);
            if ((config.cluster.va_mode.has_value() and not config.cluster.va_mode.value().fixed_k)
                or config.cluster.write_dist_matrix){
                clustering_result._distance_matrix_opt = clara_clustering.return_distance_matrix();
            }
            break;
        }

        case ClusteringConfig::ClusteringAlgorithm::CLARANS: {
            CLARANS_Clustering clarans_clustering(graphs, config, out);
            global_log << "\nRunning CLARANS Clustering (k=" << k 
                       << ") on " << graphs.size() << " graphs on " << config.pam.num_threads_matrix 
                       << " threads and " << config.clarans.num_local << " starting samples." << std::endl;
            clustering_result =  clarans_clustering.cluster(k, config.clarans);
            if ((config.cluster.va_mode.has_value() and not config.cluster.va_mode.value().fixed_k)
                or config.cluster.write_dist_matrix){
                clustering_result._distance_matrix_opt = clarans_clustering.return_distance_matrix();
            }
            break;
        }

        case ClusteringConfig::ClusteringAlgorithm::PAM: {
            PAM_Clustering pam_clustering(graphs, config, out);
            global_log << "\nRunning PAM Clustering (k=" << k 
                       << ") on " << graphs.size() << " graphs on " << config.pam.num_threads_matrix 
                       << " threads." << std::endl;
            clustering_result =  pam_clustering.cluster(config, k);
            if ((config.cluster.va_mode.has_value() and not config.cluster.va_mode.value().fixed_k)
                or config.cluster.write_dist_matrix){
                clustering_result._distance_matrix_opt = pam_clustering.return_distance_matrix();
            }
            break;
        }
        default:
            throw std::logic_error("Unhandled clustering algorithm");
    }
    return clustering_result;
}


// Iteratively computes clustering (with diminishing va) on pervious clusters
ClusteringResult run_clustering_va_mode(std::vector<VAGraph> &graph_base, Config const &config, OutputContext &out)
{
    std::cout << "Running VA mode clustering:" << std::endl;
    global_log << "Running VA mode clustering:" << std::endl;
    // prepare output
    ClusteringResult final_clustering;

    auto const starting_va = config.cluster.va_mode.value().starting_va;

    // prepare first input data
    std::vector<const GeometricGraph*> input_graphs;
    std::vector<GraphID> instance_ids;
    for (GraphID i = 0; i < graph_base.size(); i++){
        input_graphs.push_back(&graph_base[i].filter(starting_va));
        instance_ids.push_back(i);
    }

    std::queue<ClusteringInput> input_queue;
    input_queue.push(ClusteringInput{input_graphs, instance_ids, starting_va, config.cluster.k});

    size_t iteration_counter = 0;
    while (not input_queue.empty() and iteration_counter <= config.cluster.va_mode.value().max_iterations){
        auto const cur_input = input_queue.front();
        input_queue.pop();
        auto const intermediate_clustering = compute_intermediate_clustering(cur_input, config, out);

        // TODO: We probably want the final clustering to happen with va 0.0
        auto const vanishing_angle = cur_input.vanishing_angle - config.cluster.va_mode.value().va_diff;
        if (vanishing_angle >= 0){
            for (ClusterID c = 0; c < intermediate_clustering.clustering_result.clusters.size(); c++){
            auto const cluster = intermediate_clustering.clustering_result.clusters[c];

                std::vector<const GeometricGraph*> graphs;
                for (auto const &id : cluster){
                    graphs.push_back(&graph_base[id].filter(vanishing_angle));
                }                
                input_queue.push(ClusteringInput{graphs, cluster, vanishing_angle, intermediate_clustering.new_ks[c]});
            }
        }
        else {
            final_clustering.append_cluster_result(intermediate_clustering.clustering_result);
        }  
        iteration_counter++;      
    }
    return final_clustering;
}


void read_symmetric_IntervalDistance_matrix_from_csv(std::vector<std::vector<IntervalDistance>> &matrix, const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::cout << "Reading IntervalDistance matrix from file " << filename << " ..." << std::endl;
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