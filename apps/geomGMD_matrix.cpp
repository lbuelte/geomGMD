#include <iostream>
#include <fstream>
#include <chrono>
#include <numeric>
#include <filesystem>
#include <algorithm>
#include <cmath>

#include "io/command_line.h"
#include "io/config.h"
#include "io/distance_matrix.h"
#include "io/output_context.h"
#include "io/run_setup.h"
#include "io/edgemap_io.h"
#include "io/graph_io.h"
#include "distance/geomGMD.h"
#include "io/solver_messages.h"

#include "distance/geomGMDSolver.h"
#include "utility/log.h"


namespace logger = geomgmd::log;

namespace {

int run(int argc, char* argv[]) {   

    auto const command_line = parse_command_line(argc, argv);
    std::string config_path = "config_dist.toml";
    if (command_line.positional.size() >= 1) {
        config_path = command_line.positional[0];
    }
    auto config = load_dist_config(config_path);

    if (command_line.overwrite) config.io.overwrite = true;

    std::filesystem::path output_path = run_output_path(config.io);
    check_previous_results(output_path, config.io.overwrite);
    auto out = OutputContext(output_path, config.io);

    std::filesystem::path log_file = log_file_path(config.io, output_path);

    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, log_file);
    for (auto const &warning : config.warnings) logger::warning(warning);
    logger::info("Writing log file to ", log_file, "\n");
    logger::info(config);

    // Log config in extra file
    copy_config(config_path, output_path);

    // Reading input graphs
    std::filesystem::path json_path = graphs_json_path(config.io);

    std::vector<GeometricGraph> geometricgraphs;
    read_graph_vector_from_json(geometricgraphs, json_path.string());

    auto const [starting_instance, num_instances] = select_instances(config.io, geometricgraphs.size());
    logger::debug("Reduced dataset: ", num_instances, " graphs starting at index ", starting_instance);

    std::vector<const GeometricGraph*> graphs;
    for (size_t i = starting_instance; i < starting_instance + num_instances; ++i) {
        graphs.push_back(&geometricgraphs[i]);
    }
    DistanceMatrix distances(graphs, config.model, config.solver, config.logging, out);
    size_t const num_threads = config.parallel.num_threads_matrix;

    logger::info("Computing distance matrix with ", num_threads, " threads ...");
    auto begin = std::chrono::high_resolution_clock::now();
    {
        // Batches of default_batch_size pairs, smaller if there are too few pairs for all threads
        size_t const total_pairs = num_instances * (num_instances - 1) / 2;
        size_t const batch_size = total_pairs / num_threads < config.parallel.default_batch_size
                ? static_cast<size_t>(std::ceil(static_cast<double>(total_pairs) / static_cast<double>(num_threads)))
                : config.parallel.default_batch_size;
        distances.compute_all(num_threads, std::max<size_t>(batch_size, 1));
    }
    auto const distance_matrix = distances.to_matrix();
    size_t const failed_pairs = distances.failed_pairs().size();

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    logger::info("Overall computation took ", (double)dur.count() / 1000.0, " [s].");

    logger::debug("Writing distance matrix to ", out.output_path);
    out.write_interval_distance_matrix(distance_matrix, starting_instance);                 

    if (failed_pairs > 0) {
        logger::error(failed_pairs, " of ", num_instances * (num_instances - 1) / 2, " distances could not be computed",
                      " (lb = ub = nan in intervaldistance_matrix.csv, see the errors above).");
        logger::close_file();
        return EXIT_FAILURE;
    }
    logger::close_file();

    return EXIT_SUCCESS;
}

} // end of anonymous namespace


int main(int argc, char* argv[])
{
    return run_main(run, argc, argv);
}
