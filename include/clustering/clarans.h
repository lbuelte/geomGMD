#ifndef POLYGONCLUSTERING_CLARANS_H
#define POLYGONCLUSTERING_CLARANS_H

#include "clustering_base.h"
#include "modGED.h"

#include <random>

class CLARANS_Clustering {
public:

    struct MedoidSwap{
        GraphID idx_removed;
        GraphID idx_added;
    };


    CLARANS_Clustering(const std::vector<const GeometricGraph*> &graphs_, Config const &config, OutputContext &out);

    //method to compute the clustering
    ClusteringResult cluster(int k, ClaransConfig const &config);

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
    int _num_clusters = -1;
    //number of data points
    int _num_graphs = -1;

    size_t _num_local;
    size_t _max_neighbors;

    size_t _num_threads = 1;

    mutable std::mt19937 _rng_local;

    //compute entry of distance matrix
    std::optional<IntervalDistance> compute_distance_matrix_entry(GraphID const g, GraphID const h);

    // Parameters for modGED objective function
    const GurobiParams _gurobi_params;

    //Output context for writing output
    OutputContext &_output_context;

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

inline std::vector<std::vector<IntervalDistance>> CLARANS_Clustering::return_distance_matrix() const
{
    return _distance_matrix;
}




#endif // POLYGONCLUSTERING_CLARANS_H