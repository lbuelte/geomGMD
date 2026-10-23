#ifndef POLYGONCLUSTERING_PAM_H
#define POLYGONCLUSTERING_PAM_H

#include "clustering_base.h"
#include "distance/geomGMDSolver.h"

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
    const ModelParameters _model;
    const SolverSettings _solver;
    const LoggingConfig _logging;

    //Output context for writing output
    OutputContext &_output_context;

    //helper functions for PAM Clustering

    //find the initial cluster centers greedily (O(k * n²))
    void build();

    //helpers for building

    //compute all distances that are not known yet (known ones, e.g. from earlier samples in CLARA, are kept),
    //with _num_threads_distance_matrix threads
    void computeEntireDistanceMatrix();

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


#endif // POLYGONCLUSTERING_PAM_H
