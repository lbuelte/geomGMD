#include "clustering/clarans.h"

#include "clustering/sampling.h"
#include "io/edgemap_io.h"
#include "distance/geomGMD.h"
#include "utility/log.h"

#include <sstream>
#include "io/solver_messages.h"

#include <queue>
CLARANS_Clustering::CLARANS_Clustering(const std::vector<const GeometricGraph*> &graphs_, Config const &config, OutputContext &out) 
    : _graphs(graphs_)
    , _distances(graphs_, config.model, config.solver, config.logging, out)
    , _num_local(config.clarans.num_local)
    , _num_clusters(config.cluster.k)
    , _model(config.model)
    , _solver(config.solver)
    , _logging(config.logging)
    , _output_context(out)
    , _rng_local()
{
    //set n
    _num_graphs = _graphs.size();

    _max_neighbors = std::max(static_cast<size_t>(_num_clusters*(_num_graphs - _num_clusters)* 0.0125), config.clarans.max_neighbors);

    if(config.clarans.random_seed_local_search) {
        _rng_local.seed(config.clarans.random_seed_local_search.value());
    } else {
        _rng_local.seed(std::random_device{}());
    }
    if (config.cluster.parallel){
        _num_threads = config.clarans.num_threads;
    }
    geomgmd::log::info("Model parameters\n", _model, "Solver settings\n", _solver);
}

ClusteringResult CLARANS_Clustering::cluster(int k, ClaransConfig const &config) 
{
    auto start = std::chrono::high_resolution_clock::now();

    //set k
    _num_clusters = k;

    geomgmd::log::info("CLARANS Clustering on ", _num_graphs, " graphs with k = ", _num_clusters);

    if (_num_graphs <= _num_clusters){
        ClusteringResult trivial_clustering;
        trivial_clustering.clustering_solution_value = 0;
        for (ClusterID i = 0; i < _num_graphs; i++){
            trivial_clustering.medoid_indices.push_back(i);
            trivial_clustering.clusters.push_back(std::vector<GraphID>());
            trivial_clustering.clusters[i].push_back(i);
        }
        geomgmd::log::debug("Returning trivial clustering from CLARA with ", _num_graphs,
                            " graphs and ", _num_clusters, " clusters!");
        return trivial_clustering;
    }

    double best_value = std::numeric_limits<double>::infinity();
    std::vector<GraphID> best_medoids;

    geomgmd::log::debug("Running ", _num_local, " iterations of outer CLARANS loop:"); 

    for (int it = 0; it < _num_local; it++)
    {
        auto init_time = std::chrono::high_resolution_clock::now();

        sample_starting_medoids(config.random_seeds_initialization[it]);
        std::ostringstream medoids_text;
        medoids_text << "Iteration " << it << ": Sampled " << _num_clusters 
                     << " starting medoids using seed " << config.random_seeds_initialization[it] << " : ";
        for (auto const &m : _medoids){
            medoids_text << m << " ";
        }
        geomgmd::log::info(medoids_text.str());

        double current_cost = swap();
        if (current_cost <= best_value){
            best_value = current_cost;
            best_medoids = _medoids;
        } else {
            _medoids = best_medoids;
        }

        auto it_time = std::chrono::high_resolution_clock::now();
        auto it_dur = std::chrono::duration_cast<std::chrono::milliseconds>(it_time - init_time);
        geomgmd::log::info("Completed CLARANS clustering iteration ", it, " in ", it_dur.count(), " ms.");
    }

    //assign elements to clusters
    auto point_assigment = assignPointsToMedoids();
    std::vector<std::vector<GraphID>> clustering(k, std::vector<GraphID>());
    for(ClusterID p=0; p<point_assigment.size(); p++) {
        clustering[point_assigment[p]].push_back(p);
    }
    
    auto solution_value = computeTotalCost(_medoids);

    geomgmd::log::debug("Computed ", [&](){
            int num_computed_entries = 0;
            auto const distance_matrix = _distances.to_matrix();
            for (GraphID i = 0; i < _num_graphs; i++){
                for (GraphID j = 0; j < _num_graphs; j++){
                    if (distance_matrix[i][j] == IntervalDistance{-1.0, -1.0}){
                        num_computed_entries++;
                    }
                }
            }
            return num_computed_entries;
        }(),
        " / ", _num_graphs * _num_graphs, " distance matrix entries.");

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    geomgmd::log::debug("Finished CLARANS Clustering in ", dur.count(), " ms.");
    geomgmd::log::debug("Best clustering has solution value ", best_value);

    return {clustering, CLARANS_Clustering::_medoids, solution_value};
}

void CLARANS_Clustering::sample_starting_medoids(std::optional<uint32_t> random_seed)
{
    _medoids = uniformly_sample_indices(_num_graphs, _num_clusters, random_seed);
}


CLARANS_Clustering::MedoidSwap CLARANS_Clustering::sample_medoid_swap(std::vector<GraphID> const &medoids) const
{
    // Randomly choose medoid to be removed
    std::uniform_int_distribution<int> distC(0, medoids.size() - 1);
    GraphID removed_idx = medoids[distC(_rng_local)];

    // Randomly choose medoid to add
    std::uniform_int_distribution<int> distS(0, _num_graphs - 1);
    GraphID added;
    do {
        added = distS(_rng_local);
    } while (std::find(medoids.begin(), medoids.end(), added) != medoids.end());

    return {removed_idx, added};
}


double CLARANS_Clustering::compute_swap_cost(
    MedoidSwap const& swap_indices,
    size_t batch_size
) {
    // === FAST PATH: NO THREADING ===
    if (_num_threads <= 1) {
        double total = 0.0;
        for (GraphID i = 0; i < _num_graphs; ++i) {
            IntervalDistance best = {
                std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity()
            };

            for (GraphID m : _medoids) {
                if (m == swap_indices.idx_removed) continue;

                auto dist_result = _distances.get(i, m);
                if (dist_result.has_value()) {
                    best = std::min(best, dist_result.value());
                }
            }

            auto dist_result = _distances.get(i, swap_indices.idx_added);
            if (dist_result.has_value()) {
                best = std::min(best, dist_result.value());
            }

            total += best.ub;
        }
        return total;
    }

    // === TASK QUEUE SETUP ===
    struct Task {
        GraphID begin;
        GraphID end;
    };

    std::queue<Task> tasks;
    std::mutex queue_mutex;

    for (GraphID i = 0; i < _num_graphs; i += batch_size) {
        tasks.push({i, std::min(i + batch_size, (size_t) _num_graphs)});
    }

    double total = 0.0;
    std::mutex total_mutex;

    // === WORKER FUNCTION ===
    auto worker = [&]() {
        double local_sum = 0.0;

        while (true) {
            Task task;

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (tasks.empty()) break;
                task = tasks.front();
                tasks.pop();
            }

            for (GraphID i = task.begin; i < task.end; ++i) {
                IntervalDistance best = {
                    std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity()
                };

                for (GraphID m : _medoids) {
                    if (m == swap_indices.idx_removed) continue;

                    auto dist_result = _distances.get(i, m);
                    if (dist_result.has_value()) {
                        best = std::min(best, dist_result.value());
                    }
                }

                auto dist_result = _distances.get(i, swap_indices.idx_added);
                if (dist_result.has_value()) {
                    best = std::min(best, dist_result.value());
                }

                local_sum += best.ub;
            }
        }

        {
            std::lock_guard<std::mutex> lock(total_mutex);
            total += local_sum;
        }

    };

    // === THREAD POOL ===
    std::vector<std::thread> threads;
    threads.reserve(_num_threads);

    for (unsigned t = 0; t < _num_threads; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    return total;
}



void CLARANS_Clustering::printProgressBar(int current, int total, int barWidth) const 
{
    double progress = static_cast<double>(current) / total;
    int pos = static_cast<int>(barWidth * progress);

    std::cout << "[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
    }

    std::cout << "] "
              << std::fixed << std::setprecision(1)
              << (progress * 100.0) << "%  ("
              << current << " / " << total << ")\r";
    std::cout.flush();
}



//swap until convergence
double CLARANS_Clustering::swap() {

    double current_cost = computeTotalCost(_medoids);
    size_t search_count = 0;

    geomgmd::log::info("Swapping medoids. We try at most ", _max_neighbors, " many swaps if there is no improvement...");
    while (search_count < _max_neighbors){
        printProgressBar(search_count, _max_neighbors);
        MedoidSwap swap_indices = sample_medoid_swap(_medoids);

        double new_cost = compute_swap_cost(swap_indices);
        if (new_cost < current_cost){
            current_cost = new_cost;
            auto removed_it = std::find(_medoids.begin(), _medoids.end(), swap_indices.idx_removed);
            if (removed_it == _medoids.end()){
                geomgmd::log::error("Error: Medoid to be removed is not actually contained in _medoids!");
            }
            *removed_it = swap_indices.idx_added; 
            search_count = 0;
        } 
        else {
            search_count++;
        }
    }
    printProgressBar(search_count, _max_neighbors);
    return current_cost;
}

// Compute current total cost
double CLARANS_Clustering::computeTotalCost(const std::vector<GraphID>& _medoids) 
{
    geomgmd::log::info("Computing total cost for ", _num_graphs, " graphs and ", _num_clusters, " clusters...");
    double total = 0.0;
    for (GraphID i = 0; i < _num_graphs; ++i) {
        IntervalDistance best = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        for (GraphID m : _medoids){
            //TODO test if cutoff is helpful here (drawback of cutoff: we might need to compute same entry again later)
            auto const dist_result = _distances.get(i, m);
            if (dist_result.has_value()){
                best = std::min(best, dist_result.value()); 
            }
        }
        total += best.ub;
        printProgressBar((i+1)*_num_clusters, _num_graphs*_num_clusters);
    }
    return total;
};

std::vector<ClusterID> CLARANS_Clustering::assignPointsToMedoids() {
    std::vector<ClusterID> assignments(_num_graphs);

    for (GraphID i = 0; i < _num_graphs; ++i) {
        IntervalDistance best_dist = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        int best_medoid = -1;

        for (ClusterID m_idx = 0; m_idx < _num_clusters; ++m_idx) {
            GraphID m = _medoids[m_idx];
            auto const dist_result = _distances.get(i, m);
            if (dist_result.has_value() and dist_result.value() < best_dist){
                best_dist = dist_result.value();
                best_medoid = m_idx;    
            }
        }
        assignments[i] = best_medoid;
    }
    return assignments;
}

