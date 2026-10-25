#ifndef GEOMGMD_CLUSTERING_PAM_H
#define GEOMGMD_CLUSTERING_PAM_H

#include "clustering_base.h"
#include "distance/geomGMDSolver.h"
#include "io/distance_matrix.h"

#include <memory>

class PAM_Clustering {
public:

    // PAM on the graphs of a view of a distance matrix (e.g. a CLARA sample); known distances are reused,
    // computed ones are stored in the matrix
    PAM_Clustering(
            DistanceView distances,
            Config const &config, 
            const size_t BATCH_SIZE_MATRIX = 10);

    //method to compute the clustering
    ClusteringResult cluster(Config const &config, int k);


private:
    //store information about the clustering
    //ids of graphs that are medoids
    std::vector<GraphID> _medoids = std::vector<GraphID>();
    //distances between the graphs to be clustered
    DistanceView _distances;
    //all distances, once computed (BUILD and SWAP read only this copy)
    std::vector<std::vector<IntervalDistance>> _distance_matrix;

    //number of centers
    int k=-1;
    //number of data points
    int n=-1;

    //number of threads for distance matrix computation
    int _num_threads_distance_matrix = 1;
    //batch size per job in matrix parallelization (one item is one matrix entry)
    const size_t _BATCH_SIZE_MATRIX = 10;

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

// PAM (k-medoids: BUILD and SWAP) on the graphs of the view
ClusteringResult cluster_pam(DistanceView distances, size_t k, Config const &config);


#endif // GEOMGMD_CLUSTERING_PAM_H
