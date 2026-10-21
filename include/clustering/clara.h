#ifndef POLYGONCLUSTERING_CLARA_H
#define POLYGONCLUSTERING_CLARA_H

#include "io/config.h"
#include "pam.h"
#include "clustering_base.h"

class CLARA_Clustering {
public:

    struct AssignmentResult{
        std::vector<ClusterID> assignment;
        double solution_value;
    };

    struct Sample{
        std::vector<const GeometricGraph*> sample_graphs;
        std::vector<GraphID> sample_indices;
    };


    CLARA_Clustering(const std::vector<const GeometricGraph*> &graphs_, Config const &config, OutputContext &out);

    //method to compute the clustering
    ClusteringResult cluster(int k, Config const &config);

    std::vector<std::vector<IntervalDistance>> return_distance_matrix() const;

private:

    //reference for graphs to be clustered
    const std::vector<const GeometricGraph*>& _graphs;

    //number of centers
    int _num_clusters = -1;
    //number of data points
    int _num_graphs = -1;

    size_t _num_samples;
    int _sample_size = -1;

    //number of threads for distance matrix computation
    int _num_threads_distance_matrix = 1;
    //number of threads for graph to cluster medoid assignment computation
    int _num_threads_assignment = 1;

    //batch size per job in matrix parallelization (one item is one matrix entry)
    const size_t BATCH_SIZE_MATRIX = 10;
    //batch size per job in assignment parallelization (one item is the assignment of one graph)
    const size_t BATCH_SIZE_ASSIGNMENT = 10;

    //Store distance matrix entries to reuse across different samples
    std::vector<std::vector<IntervalDistance>> _distance_matrix;

    //Parameters for geomGMD objective function
    const ModelParameters _model;
    const SolverSettings _solver;
    const LoggingConfig _logging;

    //Output context for writing output
    OutputContext &_output_context;

    //helper functions for CLARA Clustering

    Sample get_sample(std::optional<uint32_t> const &rs) const;

    //method to set a fixed sample size
    void set_sample_size(ClaraConfig::SampleSizeSpec const &s, std::optional<size_t> const &min_sample_size);

    bool _reuse_distances;
    void transfer_sample_distances_to_matrix(std::vector<std::vector<IntervalDistance>> const &sample_matrix, Sample const &sample);
    std::vector<std::vector<IntervalDistance>> init_sample_matrix_from_distances(Sample const &sample);

    //wrapper function for assignment of graphs to sample medoids, calls single- or multithreaded version depending on
    //_num_threads_assignment
    AssignmentResult assign_graphs_to_sample_medoids(
            std::vector<const GeometricGraph*> const &medoids, 
            std::vector<GraphID> const &medoid_ids, 
            Sample const &sample);
    AssignmentResult assign_graphs_to_sample_medoids_singlethreaded(
            std::vector<const GeometricGraph*> const &medoids, 
            std::vector<GraphID> const &medoid_ids, 
            Sample const &sample, 
            std::vector<ClusterID>& order_of_centers, 
            std::vector<bool>& is_sampled);
    AssignmentResult assign_graphs_to_sample_medoids_multithreaded(
            std::vector<const GeometricGraph*> const &medoids, 
            std::vector<GraphID> const &medoid_ids, 
            Sample const &sample, 
            std::vector<ClusterID>& order_of_centers, 
            std::vector<bool>& is_sampled);

    // function merges cluster on sample and assignment of rest to one coherent clustering result
    ClusteringResult build_clustering_result_from_sample_cluster_and_assignment(
            std::pair<ClusteringResult, CLARA_Clustering::AssignmentResult> const &clustering, 
            Sample const &sample) const;

    //helper function to print current progress in the distance matrix computation as it is
    //the longest operation throughout the clustering process
    void printProgressBar(int current, int total, int barWidth=50) const;

};

inline std::vector<std::vector<IntervalDistance>> CLARA_Clustering::return_distance_matrix() const
{
    return _distance_matrix;
}

#endif // POLYGONCLUSTERING_CLARA_H