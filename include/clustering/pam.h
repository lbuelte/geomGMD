#ifndef POLYGONCLUSTERING_PAM_H
#define POLYGONCLUSTERING_PAM_H

#include "clustering_base.h"
#include "geomGMDSolver.h"

class PAM_Clustering {
public:

    PAM_Clustering(
            const std::vector<const GeometricGraph*> &graphs, 
            Config const &config, 
            OutputContext &out, 
            const size_t BATCH_SIZE_MATRIX = 10);

    PAM_Clustering(
            const std::vector<const GeometricGraph*> &graphs, 
            std::vector<std::vector<IntervalDistance>> &distance_matrix, 
            Config const &config, 
            OutputContext &out, 
            const size_t BATCH_SIZE_MATRIX = 10);

    //method to compute the clustering
    ClusteringResult cluster(Config const &config, int k);

    std::vector<std::vector<IntervalDistance>> return_distance_matrix() const;

private:
    //store information about the clustering
    //ids of graphs that are medoids
    std::vector<GraphID> _medoids = std::vector<GraphID>();
    //reference for graphs to be clustered
    const std::vector<const GeometricGraph*>& _graphs;
    //store already computed distances
    std::vector<std::vector<IntervalDistance>> _distance_matrix;

    //number of centers
    int k=-1;
    //number of data points
    int n=-1;

    //number of threads for distance matrix computation
    int _num_threads_distance_matrix = 1;
    //batch size per job in matrix parallelization (one item is one matrix entry)
    const size_t _BATCH_SIZE_MATRIX = 10;

    //Parameters for geomGMD objective function
    const GurobiParams _gurobi_params;
    const LoggingConfig _logging;

    //Output context for writing output
    OutputContext &_output_context;

    //helper functions for PAM Clustering

    //find the initial cluster centers greedily (O(k * n²))
    void build(IOConfig const &io_config);

    //helpers for building

    //this function allows for the option that we have already computed some of the distances
    //which e.g. happens when we call PAM multiple times from CLARA
    void compute_distance_matrix_entry(GraphID const g, GraphID const h);

    //compute entire distance matrix (wrapper function to either call single- or multithreaded variant depending
    //on _num_threads_distance_matrix
    void computeEntireDistanceMatrix();
    void computeEntireDistanceMatrixSingleThreaded();
    void computeEntireDistanceMatrixMultiThreaded();

    //helper function to print current progress in the distance matrix computation as it is
    //the longest operation throughout the clustering process
    void printProgressBar(GraphID current, GraphID total, int barWidth=50);

    //swap until convergence
    void swap();

    //helper function to compute cost given a set of medoids
    double computeTotalCost(const std::vector<GraphID>& medoids);

    //helper to assign points to medoids for returning the clustering
    std::vector<ClusterID> assignPointsToMedoids();

};

inline std::vector<std::vector<IntervalDistance>> PAM_Clustering::return_distance_matrix() const {
    return _distance_matrix;
}


#endif //POLYGONCLUSTERING_PAM_H
