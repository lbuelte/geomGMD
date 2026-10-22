#include <iostream>
#include <fstream>
#include <filesystem>

#include "io/command_line.h"
#include "io/config.h"
#include "io/edgemap_io.h"
#include "io/read_run_results.h"
#include "io/graph_io.h"
#include "io/run_setup.h"
#include "io/edgemap_svg.h"
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
    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, std::nullopt);
    for (auto const &warning : config.warnings) logger::warning(warning);
    logger::info(config);

    std::filesystem::path input_path = run_output_path(config.io);

    // Reading input graphs
    std::filesystem::path json_path = graphs_json_path(config.io);

    std::vector<GeometricGraph> geometricgraphs;
    read_graph_vector_from_json(geometricgraphs, json_path.string());

    // Model parameters of the run (for the cost breakdown in the plots), from the config copy in the run folder.
    // Only the model parameters are read, so config copies of older versions with other keys still work.
    std::filesystem::path original_config_path = input_path / "config_input.toml";
    if (not std::filesystem::exists(original_config_path)) {
        throw std::runtime_error("Could not open " + original_config_path.string()
                                 + " (written by geomGMD_matrix with the same dataset, output_path and run_name)");
    }
    auto const [model_parameters, missing_keys] = load_model_parameters(original_config_path.string());
    if (not missing_keys.empty()) {
        std::string keys;
        for (auto const &key : missing_keys) keys += (keys.empty() ? "" : ", ") + key;
        logger::warning(original_config_path, " does not set [Gurobi] ", keys,
                              ": assuming the current default (cost_factor_angle_difference: 1.0, but 2.0 in the sigspatial26 version)");
    }

    // Get reduced dataset (depending on io parameters)
    auto const [starting_instance, num_instances] = select_instances(config.io, geometricgraphs.size());
    logger::info("Reduced dataset: ", num_instances, " graphs starting at index ", starting_instance);
    
    auto const edgemaps = read_edgemaps_file(input_path / "edgemaps.json");
    auto const distances = read_distance_matrix_file(input_path / "intervaldistance_matrix.csv");
    std::filesystem::path edgemap_output_path = input_path / "edgemaps";
    std::filesystem::create_directories(edgemap_output_path);

    size_t num_plots = 0;
    for (GraphID g = starting_instance; g < starting_instance + num_instances; ++g){
        for (GraphID h = g + 1; h < starting_instance + num_instances; ++h){
            auto const stored = edgemaps.find(graph_pair(g, h));
            if (stored == edgemaps.end()) continue;
            EdgeMap const &edgemap = stored->second.edge_map;
            std::optional<double> geomGMD_dist;
            if (auto const d = distances.find(graph_pair(g, h)); d != distances.end()) {
                geomGMD_dist = d->second.ub;
            }

            // Edge maps are stored in the computation's (g, h) order; files written before edge maps
            // were reported in the caller's order may contain the pair as (h, g)
            GraphID const first = static_cast<GraphID>(edgemap.g_graph_id);
            GraphID const second = static_cast<GraphID>(edgemap.h_graph_id);
            if (first >= geometricgraphs.size() or second >= geometricgraphs.size()) {
                throw std::runtime_error("Edge map between graphs " + std::to_string(first) + " and "
                                         + std::to_string(second) + " does not fit the dataset ("
                                         + std::to_string(geometricgraphs.size()) + " graphs)");
            }
            SVGLegendInfo svg_info {stored->second.optimal, config.io.dataset, first, second, model_parameters, geomGMD_dist};
            write_edge_mapping_svg(geometricgraphs[first], geometricgraphs[second], edgemap, svg_info,
                                   edgemap_output_path.string() + "/edgemap_"
                                   + std::to_string(first) + "_" + std::to_string(second) + ".svg");
            ++num_plots;
        }
    }
    logger::info("Wrote ", num_plots, " edge map plots to ", edgemap_output_path);
    if (num_plots == 0) {
        logger::warning("No edge maps of the selected graphs found in ", input_path / "edgemaps.json");
    }

    return EXIT_SUCCESS;
}

} // end of anonymous namespace


int main(int argc, char* argv[])
{
    return run_main(run, argc, argv);
}
