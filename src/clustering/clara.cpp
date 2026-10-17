#include "clustering/clara.h"

#include "utility/sampling.h"
#include "io/logger.h"
#include "io/solver_messages.h"
#include "utility/overloaded.h"

#include <cassert>
#include <queue>
CLARA_Clustering::CLARA_Clustering(const std::vector<const GeometricGraph*> &graphs_, Config const &config, OutputContext &out) 
    : _graphs(graphs_) 
    , _gurobi_params(GurobiParams::from_config(config.gurobi))
    , _num_samples(config.clara.num_samples)
    , _output_context(out)
    , _num_clusters(config.cluster.k)
    , _reuse_distances(config.clara.reuse_distances)
{
    //set n
    this->_num_graphs = _graphs.size();

    if (config.cluster.parallel){
        _num_threads_distance_matrix = config.pam.num_threads_matrix;
        _num_threads_assignment = config.clara.num_threads_assignment;
    }
    // Sample size from original paper 80 + 2*num_clusters
    set_sample_size(config.clara.sample_size, config.clara.min_sample_size);    
    if (config.clara.reuse_distances){
        _distance_matrix = std::vector<std::vector<IntervalDistance>>(_num_graphs, std::vector<IntervalDistance>(_num_graphs, {-1.0, -1.0}));
    }

    _gurobi_params.log(std::cout);
    _gurobi_params.log(global_log);
}



ClusteringResult CLARA_Clustering::cluster(int k, Config const &config)
{
    _num_clusters = k;
    auto start = std::chrono::high_resolution_clock::now();

    std::cout << "CLARA Clustering on " << _num_graphs << " graphs with " << _num_samples 
              << " samples and k = " << _num_clusters << std::endl;

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
        std::cout << "Returning trivial clustering from CLARA with " << _num_graphs 
                  << " graphs and " << _num_clusters << " clusters!\n";
        return trivial_clustering;
    }
    if (_sample_size >= _num_graphs){
        global_log << "CLARA sample size >= number of graphs: Running PAM on the whole dataset.\n";
        std::cout << "CLARA sample size >= number of graphs: Running PAM on the whole dataset.\n";
        PAM_Clustering pam_clustering(_graphs, config, _output_context);
        return pam_clustering.cluster(config, _num_clusters);
    }

    global_log << "\nStarted CLARA Clustering with " << _num_samples << " samples:\n";

    std::pair<ClusteringResult, AssignmentResult> best_clustering;
    double best_clustering_value = std::numeric_limits<double>::infinity();
    Sample best_sample;
    size_t best_it = 0;

    for (int s = 0; s < _num_samples; s++){

        auto init_time = std::chrono::high_resolution_clock::now();
        global_log << "\nSample " << s << ":\n";

        Sample sample = get_sample(config.clara.random_seeds[s]);       

        //Compute Clustering on sample
        ClusteringResult sample_cluster_result;
        if (config.clara.reuse_distances){
            auto sample_matrix = init_sample_matrix_from_distances(sample);
            PAM_Clustering pam_clustering(sample.sample_graphs, sample_matrix, config, _output_context);
            sample_cluster_result = pam_clustering.cluster(config, _num_clusters);
            transfer_sample_distances_to_matrix(pam_clustering.return_distance_matrix(), sample);
        } else {
            PAM_Clustering pam_clustering(sample.sample_graphs, config, _output_context);
            sample_cluster_result = pam_clustering.cluster(config, _num_clusters);        
        }
        auto sample_clusters = sample_cluster_result.clusters;      

        auto sampled_time = std::chrono::high_resolution_clock::now();
        auto sampled_dur = std::chrono::duration_cast<std::chrono::milliseconds>(sampled_time - init_time);
        global_log << "Completed clustering of sampled set " << s << ". It took " << sampled_dur.count() << " ms.\n";

        // Store medoids as graphs
        std::vector<const GeometricGraph*> medoids;   
        std::vector<GraphID> medoid_indices;     
        for(int cluster_id = 0; cluster_id < sample_clusters.size(); cluster_id++) {
            medoids.push_back(sample.sample_graphs[sample_cluster_result.medoid_indices[cluster_id]]); 
            medoid_indices.push_back(sample.sample_indices[sample_cluster_result.medoid_indices[cluster_id]]);
        }

        //Assign other graphs to computed medoids
        AssignmentResult assignment = assign_graphs_to_sample_medoids(medoids, medoid_indices, sample);

        auto assigned_time = std::chrono::high_resolution_clock::now();
        auto assign_dur = std::chrono::duration_cast<std::chrono::milliseconds>(assigned_time - sampled_time);
        global_log << "Completed assignment of graphs to medoids of sample " << s << ". It took " << assign_dur.count() << " ms.\n"
                   << "Total cost of this sample clustering: " << sample_cluster_result.clustering_solution_value << " + " 
                   << assignment.solution_value << " = " << sample_cluster_result.clustering_solution_value + assignment.solution_value <<  "\n";

        // Compare to results from previous samples
        if (sample_cluster_result.clustering_solution_value + assignment.solution_value < best_clustering_value){
            best_clustering_value = sample_cluster_result.clustering_solution_value + assignment.solution_value;
            best_clustering = {sample_cluster_result, assignment};
            best_sample = sample;
            best_it = s;
        }   
        auto it_end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(it_end - init_time);
        global_log << "Iteration " << s << " took " << dur.count() << " ms in total.\n";

    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    global_log << "\nFinished CLARA Clustering in " << dur.count() << " ms.\n"
               << "Best clustering with solution value " << best_clustering_value << " based on sample " << best_it << "\n";

    return build_clustering_result_from_sample_cluster_and_assignment(best_clustering, best_sample);

}

void CLARA_Clustering::transfer_sample_distances_to_matrix(std::vector<std::vector<IntervalDistance>> const &sample_matrix, Sample const &sample)
{
    for (GraphID i = 0; i < sample_matrix.size(); i++){
        GraphID g = sample.sample_indices[i];
        for (GraphID j = 0; j < sample_matrix[i].size(); j++){
            GraphID h = sample.sample_indices[j];

            _distance_matrix[g][h] = sample_matrix[i][j];
        }
    }
}

std::vector<std::vector<IntervalDistance>> CLARA_Clustering::init_sample_matrix_from_distances(Sample const &sample)
{
    std::vector<std::vector<IntervalDistance>> sample_matrix(_sample_size, std::vector<IntervalDistance>(_sample_size, {-1.0, -1.0}));
    for (GraphID i = 0; i < _sample_size; i++){
        GraphID g = sample.sample_indices[i];
        for (GraphID j = 0; j < _sample_size; j++){
            GraphID h = sample.sample_indices[j];

            sample_matrix[i][j] = _distance_matrix[g][h];
        }
    }
    return sample_matrix;
}


void CLARA_Clustering::set_sample_size(ClaraConfig::SampleSizeSpec const &s, std::optional<size_t> const &min_sample_size)
{
    //Default from original PAM paper
    size_t DEFAULT_SAMPLE_SIZE = 80 + 2* _num_clusters;

    _sample_size = std::visit(overloaded{
        [&](std::monostate) {
            return DEFAULT_SAMPLE_SIZE;
        },
        [&](const ClaraConfig::FixedSample& f) {
            return f;
        },
        [&](const ClaraConfig::PercentageSample& p) {
            return std::max<size_t>(
                1, static_cast<size_t>(p * _num_graphs));
        }
    }, s);
    if (min_sample_size.has_value()){
        _sample_size = std::max<size_t>(_sample_size, min_sample_size.value());
    }
}


CLARA_Clustering::Sample CLARA_Clustering::get_sample(std::optional<uint32_t> const &rs) const
{
    auto sample_indices = uniformly_sample_indices(_num_graphs, _sample_size, rs);

    global_log << "Sampled " << _sample_size << " of " << _num_graphs;
    if (rs){
        global_log << " with seed " << rs.value() << " : ";
    } else {
        global_log << " : ";
    }
    
    // Prepare reduced dataset
    std::vector<const GeometricGraph*> sample_graphs;
    for(int i = 0; i < _sample_size; i++){
        int graph_id = sample_indices[i];
        sample_graphs.push_back(_graphs[graph_id]);
        global_log << graph_id << " ";
    }
    global_log << "\n";

    return {sample_graphs, sample_indices};
}


std::vector<ClusterID> sort_center_indices_by_graph_size(std::vector<const GeometricGraph*> const &centers)
{
    std::vector<ClusterID> sorted_cluster_indices(centers.size());
    for(ClusterID i = 0; i < centers.size(); i++){
        sorted_cluster_indices[i] = i;
    }
    std::sort(sorted_cluster_indices.begin(), sorted_cluster_indices.end(), [&](int a, int b){
        return centers[a]->num_vertices() < centers[b]->num_vertices();
    });
    return sorted_cluster_indices;
}


CLARA_Clustering::AssignmentResult CLARA_Clustering::assign_graphs_to_sample_medoids(
        std::vector<const GeometricGraph*> const &medoids,
        std::vector<GraphID> const& medoid_indices,
        Sample const& sample)
{
    // We want to process the "smaller" medoids first
    std::vector<ClusterID> order_of_centers =
        sort_center_indices_by_graph_size(medoids);

    std::cout << "Computing assignment of graphs to centers of sampled clustering using " << _num_threads_assignment << " threads ..."
              << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    const GraphID n = _graphs.size();
    const size_t total_graphs_to_assign =
        _graphs.size() - sample.sample_graphs.size();

    // Track sampled graphs for O(1) lookup
    std::vector<bool> is_sampled(n, false);
    for (GraphID g : sample.sample_indices) {
        is_sampled[g] = true;
    }

    AssignmentResult assignment_result;

    // === NAIVE PATH: SINGLE THREAD ===
    if (_num_threads_assignment <= 1) {
        assignment_result = assign_graphs_to_sample_medoids_singlethreaded(medoids, medoid_indices, sample, order_of_centers, is_sampled);
    }
    // === PARALLEL PATH ===
    else {
        assignment_result = assign_graphs_to_sample_medoids_multithreaded(medoids, medoid_indices, sample, order_of_centers, is_sampled);
    }

    printProgressBar(
        total_graphs_to_assign * medoids.size(),
        total_graphs_to_assign * medoids.size()
    );

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::endl
              << "Assignment computation using " << _num_threads_assignment << " threads complete, it took "
              << duration.count() << " ms." << std::endl;

    global_log << std::endl
               << "Assignment computation using "<< _num_threads_assignment << " threads took "
               << duration.count() << " ms\n";

    return assignment_result;
}

CLARA_Clustering::AssignmentResult CLARA_Clustering::assign_graphs_to_sample_medoids_singlethreaded(
        std::vector<const GeometricGraph*> const& medoids, 
        std::vector<GraphID> const& medoid_indices,
        Sample const& sample, 
        std::vector<ClusterID>& order_of_centers, 
        std::vector<bool>& is_sampled)
{
    int processed_graphs = 0;
    const GraphID n = _graphs.size();
    const size_t total_graphs_to_assign =
        _graphs.size() - sample.sample_graphs.size();

    std::vector<ClusterID> cluster_assignment(n);
    double total_cost_of_assignment = 0.0;

    for (GraphID g = 0; g < n; ++g) {
        if (is_sampled[g]) {
            continue;
        }

        IntervalDistance min_dist = {
            std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()
        };
        ClusterID min_idx = 0;

        for (ClusterID m = 0; m < medoids.size(); ++m) {

            std::optional<IntervalDistance> dist_result;
            if (_reuse_distances and _distance_matrix[g][medoid_indices[order_of_centers[m]]].ub > -1.0){
                // assert(_distance_matrix[medoid_indices[order_of_centers[m]]][g].ub > -1.0);
                dist_result = _distance_matrix[g][medoid_indices[order_of_centers[m]]];
            } else {
                GeomGMDForiSolver solver(
                        *_graphs[g],
                        *medoids[order_of_centers[m]],
                        _gurobi_params,
                        min_dist.ub);
                auto res = solver.run_optimization();

                dist_result = evaluate_distance_result(res);
                if (!dist_result.has_value()) {
                    report_solver_failure(res);
                    continue;
                }
                if (not _distance_matrix.empty()){
                    _distance_matrix[g][medoid_indices[order_of_centers[m]]] = dist_result.value();
                    _distance_matrix[medoid_indices[order_of_centers[m]]][g] = dist_result.value();
                }
                _output_context.write_edgemap(solver.return_edge_map_json());
            }            

            if (dist_result.value() < min_dist) {
                min_dist = dist_result.value();
                min_idx = order_of_centers[m];
            }
        }

        cluster_assignment[g] = min_idx;
        total_cost_of_assignment += min_dist.ub;

        ++processed_graphs;
        printProgressBar(
            processed_graphs * medoids.size(),
            total_graphs_to_assign * medoids.size()
        );
    }
    return {cluster_assignment, total_cost_of_assignment};
}

CLARA_Clustering::AssignmentResult CLARA_Clustering::assign_graphs_to_sample_medoids_multithreaded(
        std::vector<const GeometricGraph*> const& medoids, 
        std::vector<GraphID> const& medoid_indices,
        Sample const& sample, std::vector<ClusterID>& order_of_centers, 
        std::vector<bool>& is_sampled)
{
    std::mutex cost_mutex;
    std::mutex progress_mutex;

    int processed_graphs = 0;
    const GraphID n = _graphs.size();
    const size_t total_graphs_to_assign =
        _graphs.size() - sample.sample_graphs.size();

    std::vector<ClusterID> cluster_assignment(n);
    double total_cost_of_assignment = 0.0;

    std::queue<std::vector<GraphID>> tasks;
    std::mutex queue_mutex;

    // Build task queue
    std::vector<GraphID> current_batch;
    current_batch.reserve(BATCH_SIZE_ASSIGNMENT);

    for (GraphID g = 0; g < n; ++g) {
        if (is_sampled[g]) continue;

        current_batch.push_back(g);
        if (current_batch.size() == BATCH_SIZE_ASSIGNMENT) {
            tasks.push(std::move(current_batch));
            current_batch.clear();
            current_batch.reserve(BATCH_SIZE_ASSIGNMENT);
        }
    }

    if (!current_batch.empty()) {
        tasks.push(std::move(current_batch));
    }

    auto worker = [&]() {
        double local_cost = 0.0;
        size_t local_processed = 0;

        while (true) {
            std::vector<GraphID> batch;

            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (tasks.empty()) break;
                batch = std::move(tasks.front());
                tasks.pop();
            }

            for (GraphID g : batch) {
                IntervalDistance min_dist = {
                    std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::infinity()
                };
                ClusterID min_idx = 0;

                for (ClusterID m = 0; m < medoids.size(); ++m) {

                    std::optional<IntervalDistance> dist_result;
                    if (_reuse_distances and _distance_matrix[g][medoid_indices[order_of_centers[m]]].ub > -1.0){
                        // assert(_distance_matrix[medoid_indices[order_of_centers[m]]][g].ub > -1.0);
                        dist_result = _distance_matrix[g][medoid_indices[order_of_centers[m]]];
                    } 
                    else {
                        GeomGMDForiSolver solver(
                                *_graphs[g],
                                *medoids[order_of_centers[m]],
                                _gurobi_params,
                                min_dist.ub);
                        auto res = solver.run_optimization();

                        dist_result = evaluate_distance_result(res);
                        if (!dist_result.has_value()) {
                            report_solver_failure(res);
                            continue;
                        }
                        if (not _distance_matrix.empty()){
                            _distance_matrix[g][medoid_indices[order_of_centers[m]]] = dist_result.value();
                            _distance_matrix[medoid_indices[order_of_centers[m]]][g] = dist_result.value();
                        }
                        _output_context.write_edgemap(solver.return_edge_map_json());
                    }

                    if (dist_result.value() < min_dist) {
                        min_dist = dist_result.value();
                        min_idx = order_of_centers[m];
                    }
                }

                cluster_assignment[g] = min_idx;
                local_cost += min_dist.ub;
                ++local_processed;
            }

            {
                std::lock_guard<std::mutex> lock(cost_mutex);
                total_cost_of_assignment += local_cost;
            }

            {
                std::lock_guard<std::mutex> lock(progress_mutex);
                processed_graphs += local_processed;
                printProgressBar(
                    processed_graphs * medoids.size(),
                    total_graphs_to_assign * medoids.size()
                );
            }

            local_cost = 0.0;
            local_processed = 0;
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    threads.reserve(_num_threads_assignment);

    for (unsigned t = 0; t < _num_threads_assignment; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    return {cluster_assignment, total_cost_of_assignment};
}

void CLARA_Clustering::printProgressBar(int current, int total, int barWidth) const 
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


ClusteringResult CLARA_Clustering::build_clustering_result_from_sample_cluster_and_assignment(std::pair<ClusteringResult, AssignmentResult> const &clustering, Sample const &sample) const
{        
    
    // Store clustering of sampled graphs
    std::vector<std::vector<GraphID>> clusters(_num_clusters);
    assert(clustering.first.clusters.size() == _num_clusters);
    for (ClusterID i = 0; i < clustering.first.clusters.size(); i++){
        for (auto graph_index_sample : clustering.first.clusters[i]){
            assert(graph_index_sample < sample.sample_indices.size());
            clusters[i].push_back(sample.sample_indices[graph_index_sample]);
        }
    }
    // Store assignment of other graphs
    size_t counter_sampled = 0;
    for (GraphID i = 0; i < _num_graphs; i++){
        auto sample_it = std::find(sample.sample_indices.begin(), sample.sample_indices.end(), i);
        if (sample_it != sample.sample_indices.end()) 
            continue; 
        
        clusters[clustering.second.assignment[i]].push_back(i);    
    }
    // Store sampled medoids with original index
    std::vector<GraphID> medoids;
    for (auto i : clustering.first.medoid_indices){
        medoids.push_back(sample.sample_indices[i]);
    }

    return {clusters, medoids, clustering.first.clustering_solution_value + clustering.second.solution_value};
}