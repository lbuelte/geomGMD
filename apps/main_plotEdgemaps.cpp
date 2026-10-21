#include <iostream>
#include <fstream>
#include <filesystem>

#include "io/command_line.h"
#include "io/config.h"
#include "io/edgemap_io.h"
#include "io/graph_io.h"
#include "io/edgemap_svg.h"
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
    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, std::nullopt);
    logger::info(config_to_string(config));

    std::filesystem::path input_path = 
            std::filesystem::path(config.io.output_path)
            / config.io.dataset
            / config.io.run_name;

    // Reading input graphs
    std::filesystem::path json_path =
            std::filesystem::path(config.io.input_path)
            / config.io.dataset
            / (config.io.dataset + "_graphs.json");

    std::vector<GeometricGraph> geometricgraphs;
    read_graph_vector_from_json(geometricgraphs, json_path.string());

    // Get original dataset parameters from logged run config
    std::filesystem::path original_config_path = input_path / "config_input.toml";
    auto const original_config = load_dist_config(original_config_path.string());
    const ModelParameters model_parameters = original_config.model;

    // Get reduced dataset (depending on io parameters)
    size_t starting_instance = config.io.starting_instance;
    size_t num_instances = config.io.num_instances.has_value() ? std::min(config.io.num_instances.value(),
                                                                          geometricgraphs.size() - starting_instance)
                                                               : geometricgraphs.size() - starting_instance;    
    assert(starting_instance + num_instances <= geometricgraphs.size());
    logger::info("Reduced dataset: ", num_instances, " graphs starting at index ", starting_instance);
    
    std::filesystem::path distance_matrix_input_file = input_path / "intervaldistance_matrix.csv";
    std::filesystem::path edgemap_input_file = input_path / "edgemaps.json";
    std::filesystem::path edgemap_output_path = input_path / "edgemaps";
    std::filesystem::create_directories(edgemap_output_path);

    for (GraphID g = starting_instance; g < starting_instance + num_instances; ++g){
        for (GraphID h = g + 1; h < starting_instance + num_instances; ++h){

            bool g_before_h;
            auto json_opt = find_graph_mapping_from_file(edgemap_input_file, g, h, g_before_h);
            auto geomGMD_dist = find_geomGMD_value_from_file(distance_matrix_input_file, g, h);
            if (json_opt.has_value()){
                bool is_optimal_mapping = json_opt.value()["optimal"];
                if (g_before_h){
                    EdgeMap edgemap = edgemap_from_json(json_opt.value());
                    SVGLegendInfo svg_info {is_optimal_mapping, config.io.dataset, g, h, model_parameters, geomGMD_dist};
                    write_edge_mapping_svg(geometricgraphs[g], geometricgraphs[h], edgemap, svg_info,
                                      edgemap_output_path.string() + "/edgemap_" 
                                      + std::to_string(geometricgraphs[g].polygon_id()) + "_" 
                                      + std::to_string(geometricgraphs[h].polygon_id()) + ".svg");
                } else {
                    EdgeMap edgemap = edgemap_from_json(json_opt.value());
                    SVGLegendInfo svg_info {is_optimal_mapping, config.io.dataset, h, g, model_parameters, geomGMD_dist};
                    write_edge_mapping_svg(geometricgraphs[h], geometricgraphs[g], edgemap, svg_info,
                                      edgemap_output_path.string() + "/edgemap_" 
                                      + std::to_string(geometricgraphs[h].polygon_id()) + "_" 
                                      + std::to_string(geometricgraphs[g].polygon_id()) + ".svg");
                }        
            }
        }
    }
    
    return EXIT_SUCCESS;
}
