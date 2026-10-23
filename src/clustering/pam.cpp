#include "clustering/pam.h"

#include "io/edgemap_io.h"
#include "distance/geomGMD.h"
#include "utility/log.h"

#include <sstream>
#include "io/distance_matrix.h"
#include "io/solver_messages.h"

#include <queue>

//CLASS PAM_Clustering

PAM_Clustering::PAM_Clustering(const std::vector<const GeometricGraph*> &graphs, Config const &config, OutputContext &out, const size_t BATCH_SIZE_MATRIX)
    : _graphs(graphs)
    , _model(config.model)
    , _solver(config.solver)
    , _logging(config.logging)
    , _output_context(out)
    , _BATCH_SIZE_MATRIX(BATCH_SIZE_MATRIX)
{
    //set n
    this->n = graphs.size();
    //initialize empty distance matrix
    _distance_matrix = std::vector<std::vector<IntervalDistance>>(n, std::vector<IntervalDistance>(n, {-1.0, -1.0}));
    if (config.cluster.parallel){
        _num_threads_distance_matrix = config.pam.num_threads_matrix;
    }
    geomgmd::log::info("Model parameters\n", _model, "Solver settings\n", _solver);
}

PAM_Clustering::PAM_Clustering(
        const std::vector<const GeometricGraph*> &graphs, 
        std::vector<std::vector<IntervalDistance>> &distance_matrix, 
        Config const &config, 
        OutputContext &out, 
        const size_t BATCH_SIZE_MATRIX)
    : _graphs(graphs)
    , _distance_matrix(distance_matrix)
    , _model(config.model)
    , _solver(config.solver)
    , _logging(config.logging)
    , _output_context(out)
    , _BATCH_SIZE_MATRIX(BATCH_SIZE_MATRIX)
{
    //set n
    this->n = graphs.size();
    //check that matrix is the correct size
    if (n > 0){
        assert(_distance_matrix.size() == n);
        for (auto const &row : _distance_matrix){
            assert(row.size() == n);
        }
    }
    if (config.cluster.parallel){
        _num_threads_distance_matrix = config.pam.num_threads_matrix;
    }
    geomgmd::log::info("Model parameters\n", _model, "Solver settings\n", _solver);
}


ClusteringResult PAM_Clustering::cluster(Config const &config, int k) 
{
    //set k
    this->k = k;

    geomgmd::log::info("PAM Clustering on ", n, " graphs with k = ", k);

    //building phase: choose init cluster centers greedily
    build();

    //swap phase: swap cluster centers until convergence
    swap();

    //assign elements to clusters
    auto point_assigment = assignPointsToMedoids();
    std::vector<std::vector<GraphID>> clustering(k, std::vector<GraphID>());
    for(int p=0; p<point_assigment.size(); p++) {
        clustering[point_assigment[p]].push_back(p);
    }
    
    auto solution_value = computeTotalCost(_medoids);
    return {clustering, PAM_Clustering::_medoids, solution_value};
}

//find the initial cluster centers greedily (O(k * n²))
void PAM_Clustering::build() {
    //first fill up entire distance matrix (I guess we need to for the naive PAM)
    computeEntireDistanceMatrix();

    //make sure no medoid is set yet
    _medoids.clear();

    // Step 2: Select initial medoids one by one
    while (_medoids.size() < k) {
        double best_cost = std::numeric_limits<double>::infinity();
        int best_candidate = -1;

        for (GraphID i = 0; i < n; ++i) {
            // skip if already a medoid
            if (std::find(_medoids.begin(), _medoids.end(), i) != _medoids.end())
                continue;

            // compute total cost if i were added as medoid
            double total_cost = 0.0;

            for (GraphID j = 0; j < n; ++j) {
                // for each point j, find distance to nearest medoid (including i)
                IntervalDistance dmin = _distance_matrix[j][i]; // candidate as medoid
                for (GraphID m : _medoids)
                    dmin = std::min(dmin, _distance_matrix[j][m]);
                total_cost += dmin.ub;
            }

            // track the candidate with the lowest total cost
            if (total_cost < best_cost) {
                best_cost = total_cost;
                best_candidate = i;
            }
        }

        // add best candidate to medoids
        _medoids.push_back(best_candidate);
    }

}


void PAM_Clustering::computeEntireDistanceMatrix()
{
    if (_distance_matrix.empty()) {
        geomgmd::log::warning("WARNING: Cannot compute values of empty distance matrix!");
        return;
    }

    geomgmd::log::info("Computing distance matrix for PAM clustering with ", _num_threads_distance_matrix, " threads ...");
    auto start = std::chrono::high_resolution_clock::now();

    // Distances that are already known (e.g. from earlier samples in CLARA) are not computed again
    DistanceMatrix distances(_graphs, _model, _solver, _logging, _output_context);
    const GraphID n = _distance_matrix.size();
    for (GraphID i = 0; i < n; ++i) {
        for (GraphID j = i + 1; j < n; ++j) {
            if (_distance_matrix[i][j].ub > -1.0) {
                distances.set(i, j, _distance_matrix[i][j]);
            }
        }
    }
    distances.compute_all(_num_threads_distance_matrix, _BATCH_SIZE_MATRIX);
    _distance_matrix = distances.to_matrix();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    const int totalPairs = (n * (n - 1)) / 2;

    geomgmd::log::info("Distance matrix computation with ", _num_threads_distance_matrix, " threads (",
                       totalPairs, " pairs) complete, it took ", duration.count(), " ms.");
}


//swap until convergence
void PAM_Clustering::swap() {
    bool improved = true;

    double current_cost = computeTotalCost(_medoids);

    // Repeat until no improving swap found
    while (improved) {
        improved = false;
        double best_cost = current_cost;
        int best_medoid = -1;
        int best_candidate = -1;

        // Try every possible swap
        for (GraphID m : _medoids) {
            for (GraphID h = 0; h < n; ++h) {
                // skip if h is already a medoid
                if (std::find(_medoids.begin(), _medoids.end(), h) != _medoids.end())
                    continue;

                // create a temporary medoid set with the swap applied
                std::vector<GraphID> new_medoids = _medoids;
                std::replace(new_medoids.begin(), new_medoids.end(), m, h);

                double new_cost = computeTotalCost(new_medoids);

                if (new_cost < best_cost) {
                    best_cost = new_cost;
                    best_medoid = m;
                    best_candidate = h;
                }
            }
        }

        // if an improving swap was found, apply it
        if (best_cost < current_cost) {
            std::replace(_medoids.begin(), _medoids.end(), best_medoid, best_candidate);
            current_cost = best_cost;
            improved = true;
        }
    }
}

// Compute current total cost
double PAM_Clustering::computeTotalCost(const std::vector<GraphID>& medoids) {
    double total = 0.0;
    for (GraphID i = 0; i < n; ++i) {
        IntervalDistance best = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        for (GraphID m : medoids)
            best = std::min(best, _distance_matrix[i][m]);
        total += best.ub;
    }
    return total;
};

std::vector<ClusterID> PAM_Clustering::assignPointsToMedoids() {
    std::vector<ClusterID> assignments(n);

    for (GraphID i = 0; i < n; ++i) {
        IntervalDistance best_dist = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        int best_medoid = -1;

        for (ClusterID m_idx = 0; m_idx < k; ++m_idx) {
            GraphID m = _medoids[m_idx];
            IntervalDistance d = _distance_matrix[i][m];
            if (d < best_dist) {
                best_dist = d;
                best_medoid = m_idx;
            }
        }
        assignments[i] = best_medoid;
    }
    return assignments;
}


//END CLASS PAM_Clustering
