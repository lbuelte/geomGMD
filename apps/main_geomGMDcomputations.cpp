#include <iostream>
#include <fstream>
#include <chrono>
#include <numeric>
#include <cassert>
#include <filesystem>
#include <queue>
#include <thread>

#include "io/command_line.h"
#include "io/config.h"
#include "io/output_context.h"
#include "io/edgemap_io.h"
#include "distance/geomGMD.h"
#include "io/solver_messages.h"

#include "distance/geomGMDSolver.h"
#include "utility/log.h"


namespace logger = geomgmd::log;

int main(int argc, char* argv[]) {   

    auto const command_line = parse_command_line(argc, argv);
    std::string config_path = "config_dist.toml";
    if (command_line.positional.size() >= 1) {
        config_path = command_line.positional[0];
    }
    auto config = load_dist_config(config_path);

    std::filesystem::path output_path = 
            std::filesystem::path(config.io.output_path)
            / config.io.dataset
            / config.io.run_name;
    auto out = OutputContext(output_path, config.io);

    std::filesystem::path log_file = config.io.global_log_file
            ? std::filesystem::path(config.io.global_log_file.value())
            : output_path / "run.log";

    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, log_file);
    logger::info("Writing log file to ", log_file, "\n");
    logger::info(config_to_string(config));

    // Log config in extra file
    std::filesystem::copy_file(
            config_path,
            output_path / "config_input.toml",
            std::filesystem::copy_options::overwrite_existing);

    // Reading input graphs
    std::filesystem::path json_path =
            std::filesystem::path(config.io.input_path)
            / config.io.dataset
            / (config.io.dataset + "_graphs.json");

    std::vector<GeometricGraph> geometricgraphs;
    read_graph_vector_from_json(geometricgraphs, json_path.string());

    size_t starting_instance = config.io.starting_instance;
    size_t num_instances = config.io.num_instances.has_value() ? std::min(config.io.num_instances.value(),
                                                                          geometricgraphs.size() - starting_instance)
                                                               : geometricgraphs.size() - starting_instance;
    
    assert(starting_instance + num_instances <= geometricgraphs.size());
    logger::debug("Reduced dataset: ", num_instances, " graphs starting at index ", starting_instance);

    std::vector<std::vector<IntervalDistance>> distance_matrix(num_instances, std::vector<IntervalDistance>(num_instances, {-1.0, -1.0}));
    size_t num_threads = config.parallel.num_threads_matrix;

    logger::info("Computing distance matrix with ", num_threads, " threads ...");
    auto begin = std::chrono::high_resolution_clock::now();
    { 
        struct Pair {
                GraphID i;
                GraphID j;
            };

        struct Task {
            std::vector<Pair> pairs;
        };

        const GraphID n = distance_matrix.size();
        const int totalPairs = (n * (n - 1)) / 2;

        const auto BATCH_SIZE_MATRIX = [&](){ 
        if (totalPairs / num_threads < config.parallel.default_batch_size){
            return static_cast<size_t>(std::ceil(static_cast<double>(totalPairs) / static_cast<double>(num_threads)));
        }
        return config.parallel.default_batch_size;
    }();

        int computedPairs = 0;
        std::mutex progress_mutex;

        std::queue<Task> tasks;
        std::mutex queue_mutex;

        // Build task queue
        Task current;
        current.pairs.reserve(BATCH_SIZE_MATRIX);

        for (GraphID i = 0; i < n; ++i) {
            for (GraphID j = i; j < n; ++j) {
                if (distance_matrix[i][j].ub > -1.0){
                    continue;
                }
                if (i == j) {
                    distance_matrix[i][j] = {0.0, 0.0};
                    continue;
                }

                current.pairs.push_back({i, j});
                if (current.pairs.size() == BATCH_SIZE_MATRIX) {
                    tasks.push(std::move(current));
                    current = Task{};
                    current.pairs.reserve(BATCH_SIZE_MATRIX);
                }
            }
        }

        if (!current.pairs.empty()) {
            tasks.push(std::move(current));
        }

        auto printProgressBar = [&](GraphID current, GraphID total, int barWidth = 50) {
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
        };

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
                    auto const r = compute_geomGMD(geometricgraphs[p.i], geometricgraphs[p.j], config.model, config.solver,
                                                   {.capture_gurobi_log = config.logging.gurobi_log});
                    report_gurobi_log(config.logging, geometricgraphs[p.i], geometricgraphs[p.j], r);
                    auto dist_result = r.distance;

                    if (!dist_result.has_value()) {
                        report_solver_failure(r.result);
                        logger::error("Error! Distance between graphs ", p.i, " and ", p.j, " could not be computed!");
                        continue;
                    }
                    out.write_computation_info_row(r);
                    out.write_edgemap(edgemap_json(r));

                    distance_matrix[p.i][p.j] = dist_result.value();
                    distance_matrix[p.j][p.i] = dist_result.value();

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
        threads.reserve(num_threads);

        for (unsigned t = 0; t < num_threads; ++t) {
            threads.emplace_back(worker);
        }

        for (auto& t : threads) {
            t.join();
        }

        printProgressBar(totalPairs, totalPairs);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    logger::info("Overall computation took ", (double)dur.count() / 1000.0, " [s].");

    logger::debug("Writing distance matrix to ", out.output_path);
    out.write_interval_distance_matrix(distance_matrix);                 

    logger::close_file();

    return EXIT_SUCCESS;
}
