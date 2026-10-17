#include "clustering/pam.h"

// #include "io/plots_and_stats.h"
#include "io/logger.h"
#include "io/solver_messages.h"

#include <queue>

//CLASS PAM_Clustering

PAM_Clustering::PAM_Clustering(const std::vector<const GeometricGraph*> &graphs, Config const &config, OutputContext &out, const size_t BATCH_SIZE_MATRIX)
    : _graphs(graphs)
    , _gurobi_params(GurobiParams::from_config(config.gurobi))
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
    _gurobi_params.log(std::cout);
    _gurobi_params.log(global_log);
}

PAM_Clustering::PAM_Clustering(
        const std::vector<const GeometricGraph*> &graphs, 
        std::vector<std::vector<IntervalDistance>> &distance_matrix, 
        Config const &config, 
        OutputContext &out, 
        const size_t BATCH_SIZE_MATRIX)
    : _graphs(graphs)
    , _distance_matrix(distance_matrix)
    , _gurobi_params(GurobiParams::from_config(config.gurobi))
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
    _gurobi_params.log(std::cout);
    _gurobi_params.log(global_log);
}


ClusteringResult PAM_Clustering::cluster(Config const &config, int k) 
{
    //set k
    this->k = k;

    std::cout << "PAM Clustering on " << n << " graphs with k = " << k << std::endl;

    //building phase: choose init cluster centers greedily
    build(config.io);

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
void PAM_Clustering::build(IOConfig const &config) {
    //first fill up entire distance matrix (I guess we need to for the naive PAM)
    computeEntireDistanceMatrix();

    // std::filesystem::path dist_matrix_path =
    //         std::filesystem::path(config.output_path)
    //         / config.dataset
    //         / config.run_name
    //         / "distance_matrix.csv";
    // _distance_matrix = read_interval_distance_matrix(dist_matrix_path.string());

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


void PAM_Clustering::compute_distance_matrix_entry(GraphID const i, GraphID const j)
{
    if (_distance_matrix.empty()) {
        std::cout << "WARNING: Cannot compute values of empty distance matrix!" << std::endl;
    }
    
    if (_distance_matrix[i][j].ub > -1.0){
        return;
    }
    if(i == j){
        _distance_matrix[i][j] = ExactDistance{0.0};
        return;
    } 
    GeomGMDForiSolver solver(*_graphs[i], *_graphs[j], _gurobi_params); 
    auto const res = solver.run_optimization();
    auto dist_result = evaluate_distance_result(res);
    if (!dist_result.has_value()) {
        report_solver_failure(res);
        std::cout << "Error! Distance between graphs "
                    << i << " and " << j << " could not be computed!\n";
        return;
    }
    _output_context.write_edgemap(solver.return_edge_map_json());
    _output_context.write_computation_info_row(solver, dist_result.value());
    _distance_matrix[i][j] = dist_result.value();
    _distance_matrix[j][i] = dist_result.value();
    return;
}


void PAM_Clustering::computeEntireDistanceMatrix()
{
    if (_distance_matrix.empty()) {
        std::cout << "WARNING: Cannot compute values of empty distance matrix!" << std::endl;
        return;
    }

    std::cout << "Computing distance matrix for PAM clustering with " << _num_threads_distance_matrix << " threads ..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    // === NAIVE PATH: SINGLE THREAD ===
    if (_num_threads_distance_matrix <= 1) {
        computeEntireDistanceMatrixSingleThreaded();
    }
    // === PARALLEL PATH ===
    else {
        computeEntireDistanceMatrixMultiThreaded();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    const GraphID n = _distance_matrix.size();
    const int totalPairs = (n * (n - 1)) / 2;

    std::cout << std::endl
              << "Distance matrix computation with " << _num_threads_distance_matrix << " threads complete, it took "
              << duration.count() << " ms." << std::endl;

    global_log << "Distance matrix computation with " << _num_threads_distance_matrix << " threads ("
               << totalPairs << " pairs) took "
               << duration.count() << " ms\n";
}

void PAM_Clustering::computeEntireDistanceMatrixSingleThreaded() {
    // info for progress bar
    int computedPairs = 0;
    const GraphID n = _distance_matrix.size();
    const int totalPairs = (n * (n - 1)) / 2;

    for (GraphID i = 0; i < n; ++i) {
        for (GraphID j = i; j < n; ++j) {
            compute_distance_matrix_entry(i, j);

            if (++computedPairs % 5 == 0 || computedPairs == totalPairs) {
                printProgressBar(computedPairs, totalPairs);
            }
            
        }
    }

    printProgressBar(totalPairs, totalPairs);
}

void PAM_Clustering::computeEntireDistanceMatrixMultiThreaded() {
    struct Pair {
            GraphID i;
            GraphID j;
        };

    struct Task {
        std::vector<Pair> pairs;
    };

    const GraphID n = _distance_matrix.size();
    const int totalPairs = (n * (n - 1)) / 2;

    int computedPairs = 0;
    std::mutex progress_mutex;

    std::queue<Task> tasks;
    std::mutex queue_mutex;

    // Build task queue
    Task current;
    current.pairs.reserve(_BATCH_SIZE_MATRIX);

    for (GraphID i = 0; i < n; ++i) {
        for (GraphID j = i; j < n; ++j) {
            if (_distance_matrix[i][j].ub > -1.0){
                continue;
            }
            if (i == j) {
                _distance_matrix[i][j] = {0.0, 0.0};
                continue;
            }

            current.pairs.push_back({i, j});
            if (current.pairs.size() == _BATCH_SIZE_MATRIX) {
                tasks.push(std::move(current));
                current = Task{};
                current.pairs.reserve(_BATCH_SIZE_MATRIX);
            }
        }
    }

    if (!current.pairs.empty()) {
        tasks.push(std::move(current));
    }

    // Worker function
    auto worker = [&]() {
        while (true) {
            Task task;

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (tasks.empty()) {
                    break;
                }
                task = std::move(tasks.front());
                tasks.pop();
            }

            for (const auto& p : task.pairs) {
                GeomGMDForiSolver solver(*_graphs[p.i], *_graphs[p.j], _gurobi_params);
                auto const res = solver.run_optimization();
                auto dist_result = evaluate_distance_result(res);

                if (!dist_result.has_value()) {
                    report_solver_failure(res);
                    std::cout << "Error! Distance between graphs "
                              << p.i << " and " << p.j << " could not be computed!\n";
                    continue;
                }
                _output_context.write_edgemap(solver.return_edge_map_json());
                _output_context.write_computation_info_row(solver, dist_result.value());

                _distance_matrix[p.i][p.j] = dist_result.value();
                _distance_matrix[p.j][p.i] = dist_result.value();

                {
                    std::lock_guard<std::mutex> lock(progress_mutex);
                    ++computedPairs;
                    if (computedPairs % 5 == 0 || computedPairs == totalPairs) {
                        printProgressBar(computedPairs, totalPairs);
                    }
                }
            }
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    threads.reserve(_num_threads_distance_matrix);

    for (unsigned t = 0; t < _num_threads_distance_matrix; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    printProgressBar(totalPairs, totalPairs);
}

void PAM_Clustering::printProgressBar(
        GraphID current, GraphID total, int barWidth
) {
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
