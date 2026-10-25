#ifndef GEOMGMD_CLUSTERING_CLARA_H
#define GEOMGMD_CLUSTERING_CLARA_H

#include "io/config.h"
#include "pam.h"
#include "io/distance_matrix.h"
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


    // CLARA on the graphs of a view of a distance matrix; known distances are reused, computed ones are stored
    CLARA_Clustering(DistanceView distances, Config const &config);

    //method to compute the clustering
    ClusteringResult cluster(int k, Config const &config);

private:

    //distances between the graphs to be clustered (shared by all samples)
    DistanceView _distances;
    //graphs to be clustered (the graphs of _distances)
    std::vector<const GeometricGraph*> _graphs;

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

    //helper functions for CLARA Clustering

    Sample get_sample(std::optional<uint32_t> const &rs) const;

    //method to set a fixed sample size
    void set_sample_size(ClaraConfig::SampleSizeSpec const &s, std::optional<size_t> const &min_sample_size);


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

// CLARA (PAM on samples, assignment of the other graphs to the sample medoids) on the graphs of the view
ClusteringResult cluster_clara(DistanceView distances, size_t k, Config const &config);

#endif // GEOMGMD_CLUSTERING_CLARA_H