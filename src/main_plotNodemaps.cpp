#include <iostream>
#include <fstream>
#include <filesystem>

#include "io/config.h"
#include "io/edgemap_io.h"
#include "modGED.h"

int main(int argc, char* argv[]) {   

    std::string config_path = "config_dist.toml";
    if (argc >= 2) {
        config_path = argv[1];
    }
    auto const config = load_dist_config(config_path);
    log_config(config, std::cout);

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
    const GurobiParams gurobi_params = GurobiParams::from_config(original_config.gurobi);

    // Get reduced dataset (depending on io parameters)
    size_t starting_instance = config.io.starting_instance;
    size_t num_instances = config.io.num_instances.has_value() ? std::min(config.io.num_instances.value(),
                                                                          geometricgraphs.size() - starting_instance)
                                                               : geometricgraphs.size() - starting_instance;    
    assert(starting_instance + num_instances <= geometricgraphs.size());
    std::cout << "Reduced dataset: " << num_instances << " graphs starting at index " << starting_instance << "\n";
    
    std::filesystem::path distance_matrix_input_file = input_path / "intervaldistance_matrix.csv";
    std::filesystem::path nodemap_input_file = input_path / "nodemaps.json";
    std::filesystem::path nodemap_output_path = input_path / "nodemaps";
    std::filesystem::create_directories(nodemap_output_path);

    for (GraphID g = starting_instance; g < starting_instance + num_instances; ++g){
        for (GraphID h = g + 1; h < starting_instance + num_instances; ++h){

            bool g_before_h;
            auto json_opt = find_graph_mapping_from_file(nodemap_input_file, g, h, g_before_h);
            auto modGED_dist = find_modGED_value_from_file(distance_matrix_input_file, g, h);
            if (json_opt.has_value()){
                bool is_optimal_mapping = json_opt.value()["optimal"];
                if (g_before_h){
                    EdgeMap edgemap(json_opt.value(), geometricgraphs[g], geometricgraphs[h]);
                    SVGLegendInfo svg_info {is_optimal_mapping, config.io.dataset, g, h, gurobi_params, modGED_dist};
                    write_edge_mapping_svg(geometricgraphs[g], geometricgraphs[h], edgemap, svg_info,
                                      nodemap_output_path.string() + "/nodemap_" 
                                      + std::to_string(geometricgraphs[g].polygon_id()) + "_" 
                                      + std::to_string(geometricgraphs[h].polygon_id()) + ".svg");
                } else {
                    EdgeMap edgemap(json_opt.value(), geometricgraphs[h], geometricgraphs[g]);
                    SVGLegendInfo svg_info {is_optimal_mapping, config.io.dataset, h, g, gurobi_params, modGED_dist};
                    write_edge_mapping_svg(geometricgraphs[h], geometricgraphs[g], edgemap, svg_info,
                                      nodemap_output_path.string() + "/nodemap_" 
                                      + std::to_string(geometricgraphs[h].polygon_id()) + "_" 
                                      + std::to_string(geometricgraphs[g].polygon_id()) + ".svg");
                }        
            }
        }
    }
    
    return EXIT_SUCCESS;
}
