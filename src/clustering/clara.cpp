#include "clustering/clara.h"

#include "clustering/sampling.h"
#include "io/edgemap_io.h"
#include "distance/geomGMD.h"
#include "utility/log.h"

#include <sstream>
#include "io/solver_messages.h"
#include "utility/overloaded.h"

#include <cassert>
#include <queue>
CLARA_Clustering::CLARA_Clustering(DistanceView distances, Config const &config) 
    : _distances(std::move(distances))
    , _num_clusters(config.cluster.k)
    , _num_samples(config.clara.num_samples)
{
    for (size_t i = 0; i < _distances.size(); ++i) {
        _graphs.push_back(&_distances.graph(i));
    }
    //set n
    this->_num_graphs = _graphs.size();

    if (config.cluster.parallel){
        _num_threads_distance_matrix = config.pam.num_threads_matrix;
        _num_threads_assignment = config.clara.num_threads_assignment;
    }
    // Sample size from original paper 80 + 2*num_clusters
    set_sample_size(config.clara.sample_size, config.clara.min_sample_size);    

    geomgmd::log::info("Model parameters\n", config.model, "Solver settings\n", config.solver);
}



ClusteringResult CLARA_Clustering::cluster(int k, Config const &config)
{
    _num_clusters = k;
    auto start = std::chrono::high_resolution_clock::now();

    geomgmd::log::info("CLARA Clustering on ", _num_graphs, " graphs with ", _num_samples,
                       " samples and k = ", _num_clusters);

    if (_num_graphs <= _num_clusters){
        ClusteringResult trivial_clustering;
        trivial_clustering.clustering_solution_value = 0;
        for (ClusterID i = 0; i < _num_graphs; i++){
            trivial_clustering.medoid_indices.push_back(i);
            trivial_clustering.clusters.push_back(std::vector<GraphID>());
            trivial_clustering.clusters[i].push_back(i);
        }
        geomgmd::log::info("Returning trivial clustering from CLARA with ", _num_graphs,
                           " graphs and ", _num_clusters, " clusters!");
        return trivial_clustering;
    }
    if (_sample_size < _num_clusters){
        throw std::runtime_error("CLARA: the sample size " + std::to_string(_sample_size) + " is smaller than k = "
                                 + std::to_string(_num_clusters) + " ([Clara] sample_size / sample_fraction / min_sample_size)");
    }
    if (_sample_size >= _num_graphs){
        geomgmd::log::info("CLARA sample size >= number of graphs: Running PAM on the whole dataset.");
        PAM_Clustering pam_clustering(_distances, config);
        return pam_clustering.cluster(_num_clusters);
    }

    geomgmd::log::debug("Started CLARA Clustering with ", _num_samples, " samples:");

    std::pair<ClusteringResult, AssignmentResult> best_clustering;
    double best_clustering_value = std::numeric_limits<double>::infinity();
    Sample best_sample;
    size_t best_it = 0;

    for (size_t s = 0; s < _num_samples; s++){

        auto init_time = std::chrono::high_resolution_clock::now();
        geomgmd::log::debug("Sample ", s, ":");

        Sample sample = get_sample(config.clara.random_seeds[s]);       

        //Compute Clustering on sample
        // PAM on the sample, with the distances shared by all samples
        PAM_Clustering pam_clustering(_distances.subview(sample.sample_indices), config);
        ClusteringResult sample_cluster_result = pam_clustering.cluster(_num_clusters);
        auto sample_clusters = sample_cluster_result.clusters;      

        auto sampled_time = std::chrono::high_resolution_clock::now();
        auto sampled_dur = std::chrono::duration_cast<std::chrono::milliseconds>(sampled_time - init_time);
        geomgmd::log::debug("Completed clustering of sampled set ", s, ". It took ", sampled_dur.count(), " ms.");

        // Store medoids as graphs
        std::vector<const GeometricGraph*> medoids;   
        std::vector<GraphID> medoid_indices;     
        for(ClusterID cluster_id = 0; cluster_id < sample_clusters.size(); cluster_id++) {
            medoids.push_back(sample.sample_graphs[sample_cluster_result.medoid_indices[cluster_id]]); 
            medoid_indices.push_back(sample.sample_indices[sample_cluster_result.medoid_indices[cluster_id]]);
        }

        //Assign other graphs to computed medoids
        AssignmentResult assignment = assign_graphs_to_sample_medoids(medoids, medoid_indices, sample);

        auto assigned_time = std::chrono::high_resolution_clock::now();
        auto assign_dur = std::chrono::duration_cast<std::chrono::milliseconds>(assigned_time - sampled_time);
        geomgmd::log::debug("Completed assignment of graphs to medoids of sample ", s, ". It took ", assign_dur.count(), " ms.");
        geomgmd::log::debug("Total cost of this sample clustering: ", sample_cluster_result.clustering_solution_value, " + ",
                            assignment.solution_value, " = ", sample_cluster_result.clustering_solution_value + assignment.solution_value);

        // Compare to results from previous samples (the first sample in any case: with infinite distances
        // all values can be infinite)
        if (s == 0 or sample_cluster_result.clustering_solution_value + assignment.solution_value < best_clustering_value){
            best_clustering_value = sample_cluster_result.clustering_solution_value + assignment.solution_value;
            best_clustering = {sample_cluster_result, assignment};
            best_sample = sample;
            best_it = s;
        }   
        auto it_end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(it_end - init_time);
        geomgmd::log::debug("Iteration ", s, " took ", dur.count(), " ms in total.");

    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    geomgmd::log::debug("Finished CLARA Clustering in ", dur.count(), " ms.");
    geomgmd::log::debug("Best clustering with solution value ", best_clustering_value, " based on sample ", best_it);

    return build_clustering_result_from_sample_cluster_and_assignment(best_clustering, best_sample);

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

    std::ostringstream sample_text;
    sample_text << "Sampled " << _sample_size << " of " << _num_graphs;
    if (rs){
        sample_text << " with seed " << rs.value() << " : ";
    } else {
        sample_text << " : ";
    }
    
    // Prepare reduced dataset
    std::vector<const GeometricGraph*> sample_graphs;
    for(size_t i = 0; i < _sample_size; i++){
        int graph_id = sample_indices[i];
        sample_graphs.push_back(_graphs[graph_id]);
        sample_text << graph_id << " ";
    }
    geomgmd::log::debug(sample_text.str());

    return {sample_graphs, sample_indices};
}


std::vector<ClusterID> sort_center_indices_by_graph_size(std::vector<const GeometricGraph*> const &centers)
{
    std::vector<ClusterID> sorted_cluster_indices(centers.size());
    for(ClusterID i = 0; i < centers.size(); i++){
        sorted_cluster_indices[i] = i;
    }
    std::sort(sorted_cluster_indices.begin(), sorted_cluster_indices.end(), [&](int a, int b){
        return centers[a]->num_nodes() < centers[b]->num_nodes();
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

    geomgmd::log::info("Computing assignment of graphs to centers of sampled clustering using ", _num_threads_assignment, " threads ...");
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

    std::cout << std::endl;  // end the progress bar line
    geomgmd::log::info("Assignment computation using ", _num_threads_assignment, " threads complete, it took ",
                       duration.count(), " ms.");
    abort_on_failed_pairs(_distances);

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

            // Known distances are reused; otherwise computed with the current minimum as cutoff
            auto const dist_result = _distances.get_with_cutoff(g, medoid_indices[order_of_centers[m]], min_dist.ub);
            if (!dist_result.has_value()) {
                continue;
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

                    // Known distances are reused; otherwise computed with the current minimum as cutoff
                    auto const dist_result = _distances.get_with_cutoff(g, medoid_indices[order_of_centers[m]], min_dist.ub);
                    if (!dist_result.has_value()) {
                        continue;
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

    for (size_t t = 0; t < _num_threads_assignment; ++t) {
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


ClusteringResult cluster_clara(DistanceView distances, size_t k, Config const &config)
{
    CLARA_Clustering clara_clustering(std::move(distances), config);
    return clara_clustering.cluster(k, config);
}
