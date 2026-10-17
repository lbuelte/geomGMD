#include "clustering/clarans.h"

#include "utility/sampling.h"
#include "io/logger.h"
#include "io/solver_messages.h"

#include <queue>
CLARANS_Clustering::CLARANS_Clustering(const std::vector<const GeometricGraph*> &graphs_, Config const &config, OutputContext &out) 
    : _graphs(graphs_)
    , _num_local(config.clarans.num_local)
    , _num_clusters(config.cluster.k)
    , _gurobi_params(GurobiParams::from_config(config.gurobi))
    , _output_context(out)
    , _rng_local()
{
    //set n
    _num_graphs = _graphs.size();
    //initialize empty distance matrix
    _distance_matrix = std::vector<std::vector<IntervalDistance>>(_num_graphs, std::vector<IntervalDistance>(_num_graphs, {-1.0, -1.0}));

    _max_neighbors = std::max(static_cast<size_t>(_num_clusters*(_num_graphs - _num_clusters)* 0.0125), config.clarans.max_neighbors);

    if(config.clarans.random_seed_local_search) {
        _rng_local.seed(config.clarans.random_seed_local_search.value());
    } else {
        _rng_local.seed(std::random_device{}());
    }
    if (config.cluster.parallel){
        _num_threads = config.clarans.num_threads;
    }
    _gurobi_params.log(std::cout);
    _gurobi_params.log(global_log);
}

ClusteringResult CLARANS_Clustering::cluster(int k, ClaransConfig const &config) 
{
    auto start = std::chrono::high_resolution_clock::now();

    //set k
    _num_clusters = k;

    std::cout << "CLARANS Clustering on " << _num_graphs << " graphs with k = " << _num_clusters << std::endl;

    if (_num_graphs <= _num_clusters){
        ClusteringResult trivial_clustering;
        trivial_clustering.clustering_solution_value = 0;
        for (ClusterID i = 0; i < _num_graphs; i++){
            trivial_clustering.medoid_indices.push_back(i);
            trivial_clustering.clusters.push_back(std::vector<GraphID>());
            trivial_clustering.clusters[i].push_back(i);
        }
        global_log << "Returning trivial clustering from CLARA with " << _num_graphs 
                   << " graphs and " << _num_clusters << " clusters!\n";
        return trivial_clustering;
    }

    double best_value = std::numeric_limits<double>::infinity();
    std::vector<GraphID> best_medoids;

    global_log << "Running " << _num_local << " iterations of outer CLARANS loop:\n"; 

    for (int it = 0; it < _num_local; it++)
    {
        auto init_time = std::chrono::high_resolution_clock::now();

        sample_starting_medoids(config.random_seeds_initialization[it]);
        global_log << "Iteration " << it << ": Sampled " << _num_clusters 
                   << " starting medoids using seed " << config.random_seeds_initialization[it] << " : ";
        std::cout << "Iteration " << it << ": Sampled " << _num_clusters 
                  << " starting medoids using seed " << config.random_seeds_initialization[it] << " : ";
        for (auto const &m : _medoids){
            global_log << m << " ";
            std::cout << m << " ";
        }
        global_log << "\n";
        std::cout << "\n";

        double current_cost = swap();
        if (current_cost <= best_value){
            best_value = current_cost;
            best_medoids = _medoids;
        } else {
            _medoids = best_medoids;
        }

        auto it_time = std::chrono::high_resolution_clock::now();
        auto it_dur = std::chrono::duration_cast<std::chrono::milliseconds>(it_time - init_time);
        global_log << "Completed CLARANS clustering iteration " << it << " in " << it_dur.count() << " ms.\n";
        std::cout << "Completed CLARANS clustering iteration " << it << " in " << it_dur.count() << " ms.\n";
    }

    //assign elements to clusters
    auto point_assigment = assignPointsToMedoids();
    std::vector<std::vector<GraphID>> clustering(k, std::vector<GraphID>());
    for(ClusterID p=0; p<point_assigment.size(); p++) {
        clustering[point_assigment[p]].push_back(p);
    }
    
    auto solution_value = computeTotalCost(_medoids);

    global_log << " Computed " << [&](){
            int num_computed_entries = 0;
            for (GraphID i = 0; i < _num_graphs; i++){
                for (GraphID j = 0; j < _num_graphs; j++){
                    if (_distance_matrix[i][j] == IntervalDistance{-1.0, -1.0}){
                        num_computed_entries++;
                    }
                }
            }
            return num_computed_entries;
        }() 
               << " / " << _num_graphs * _num_graphs << " distance matrix entries.\n";

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    global_log << "\nFinished CLARANS Clustering in " << dur.count() << " ms.\n"
               << "Best clustering has solution value " << best_value << "\n";

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


std::optional<IntervalDistance> CLARANS_Clustering::compute_distance_matrix_entry(GraphID const i, GraphID const j) 
{
    if (_distance_matrix.empty()) {
        std::cout << "WARNING: Cannot compute values of empty distance matrix!" << std::endl;
        return std::nullopt;
    }
    
    if (_distance_matrix[i][j].ub > -1.0){
        return _distance_matrix[i][j];
    }
    if(i == j){
        _distance_matrix[i][j] = ExactDistance{0.0};
        return _distance_matrix[i][j];
    }    
    GeomGMDForiSolver solver(*_graphs[i], *_graphs[j], _gurobi_params);  
    auto const res = solver.run_optimization();
    auto dist_result = evaluate_distance_result(res);
    if (!dist_result.has_value()) {
        report_solver_failure(res);
        std::cout << "Error! Distance between graphs "
                    << i << " and " << j << " could not be computed!\n";
        return std::nullopt;
    }
    _distance_matrix[i][j] = dist_result.value();
    _distance_matrix[j][i] = dist_result.value();
    _output_context.write_edgemap(solver.return_edge_map_json());
    return _distance_matrix[i][j];
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

                auto dist_result = compute_distance_matrix_entry(i, m);
                if (dist_result.has_value()) {
                    best = std::min(best, dist_result.value());
                }
            }

            auto dist_result = compute_distance_matrix_entry(i, swap_indices.idx_added);
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

                    auto dist_result = compute_distance_matrix_entry(i, m);
                    if (dist_result.has_value()) {
                        best = std::min(best, dist_result.value());
                    }
                }

                auto dist_result = compute_distance_matrix_entry(i, swap_indices.idx_added);
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

    std::cout << "Swapping medoids. We try at most " << _max_neighbors << " many swaps if there is no improvement...\n";
    while (search_count < _max_neighbors){
        printProgressBar(search_count, _max_neighbors);
        MedoidSwap swap_indices = sample_medoid_swap(_medoids);

        double new_cost = compute_swap_cost(swap_indices);
        if (new_cost < current_cost){
            current_cost = new_cost;
            auto removed_it = std::find(_medoids.begin(), _medoids.end(), swap_indices.idx_removed);
            if (removed_it == _medoids.end()){
                std::cout << "Error: Medoid to be removed is not actually contained in _medoids!" << std::endl;
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
    std::cout << "Computing total cost for " << _num_graphs << " graphs and " << _num_clusters << " clusters...\n";
    double total = 0.0;
    for (GraphID i = 0; i < _num_graphs; ++i) {
        IntervalDistance best = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        for (GraphID m : _medoids){
            //TODO test if cutoff is helpful here (drawback of cutoff: we might need to compute same entry again later)
            auto const dist_result = compute_distance_matrix_entry(i, m);
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
            auto const dist_result = compute_distance_matrix_entry(i, m);
            if (dist_result.has_value() and dist_result.value() < best_dist){
                best_dist = dist_result.value();
                best_medoid = m_idx;    
            }
        }
        assignments[i] = best_medoid;
    }
    return assignments;
}

