#ifndef POLYGONCLUSTERING_CLARANS_H
#define POLYGONCLUSTERING_CLARANS_H

#include "clustering_base.h"
#include "distance/geomGMDSolver.h"
#include "io/distance_matrix.h"

#include <random>

class CLARANS_Clustering {
public:

    struct MedoidSwap{
        GraphID idx_removed;
        GraphID idx_added;
    };


    // CLARANS on the graphs of a view of a distance matrix; distances are computed when they are needed
    CLARANS_Clustering(DistanceView distances, Config const &config);

    //method to compute the clustering
    ClusteringResult cluster(int k, ClaransConfig const &config);

private:
    //store information about the clustering
    //ids of graphs that are medoids
    std::vector<GraphID> _medoids = std::vector<GraphID>();
    //distances between the graphs to be clustered, computed on demand
    DistanceView _distances;

    //number of centers
    int _num_clusters = -1;
    //number of data points
    int _num_graphs = -1;

    size_t _num_local;
    size_t _max_neighbors;

    size_t _num_threads = 1;

    mutable std::mt19937 _rng_local;


    void sample_starting_medoids(std::optional<uint32_t> rs);

    // swap randomly chosen medoids until trying max_neighbors yields no better solution
    // returns value of best found solution
    double swap();

    MedoidSwap sample_medoid_swap(std::vector<GraphID> const &medoids) const;

    double compute_swap_cost(MedoidSwap const &swap_indices, size_t batch_size = 10);

    void printProgressBar(int current, int total, int barWidth=50) const;

    //helper function to compute cost given a set of medoids
    double computeTotalCost(const std::vector<GraphID>& medoids);

    //helper to assign points to medoids for returning the clustering
    std::vector<ClusterID> assignPointsToMedoids();

};

// CLARANS (randomized search for medoid swaps) on the graphs of the view
ClusteringResult cluster_clarans(DistanceView distances, size_t k, Config const &config);




#endif // POLYGONCLUSTERING_CLARANS_H