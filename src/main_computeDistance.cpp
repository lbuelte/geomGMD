#include <iostream>
#include <fstream>
#include <chrono>
#include <cassert>
#include <filesystem>

#include "io/logger.h"
#include "io/config.h"
#include "io/edgemap_io.h"

#include "modGED.h"


int main(int argc, char* argv[]) 
{  
    if (argc < 3){
        throw std::runtime_error("Error: Need graph IDs as input arguments");
    }
    GraphID g,h;
    try {
        g = std::stoi(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "Invalid integer: " << argv[1] << std::endl;
    }
    try {
        h = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "Invalid integer: " << argv[2] << std::endl;
    }    

    std::string config_path = "config_dist.toml";
    if (argc >= 4) {
        config_path = argv[3];
    }
    auto const config = load_dist_config(config_path);
    log_config(config, std::cout);

    const GurobiParams gurobi_params = GurobiParams::from_config(config.gurobi);
    gurobi_params.log(std::cout);

    std::filesystem::path output_path = 
            std::filesystem::path(config.io.output_path)
            / config.io.dataset
            / config.io.run_name;
    std::filesystem::create_directories(output_path);

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

    assert(g < geometricgraphs.size() and h < geometricgraphs.size());
    GeometricGraph &G = geometricgraphs[g];
    GeometricGraph &H = geometricgraphs[h];

    auto begin = std::chrono::high_resolution_clock::now();
    
    modGEDForiSolver modGED(geometricgraphs[g], geometricgraphs[h], gurobi_params);
    auto dist_result = evaluate_distance_result(modGED.run_optimization());

    if (!dist_result.has_value()) {
        throw std::runtime_error ("Error! Distance between graphs " + std::to_string(g) +
                                  " and " + std::to_string(h) + " could not be computed!");
    }
    std::filesystem::path nodemap_output_file = 
            output_path 
            / ("nodemap_" + std::to_string(H.polygon_id()) + "_" + std::to_string(G.polygon_id()) + ".svg");
    
    auto nodemap = modGED.return_node_map_json();  
    modGED.print_objective_breakdown();
    bool is_optimal_mapping = nodemap["optimal"];
    if (H <= G){
        assert(nodemap["G_id"] == g and nodemap["H_id"] == h);
        EdgeMap edgemap(nodemap, G, H);
        SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, g, h, gurobi_params, dist_result.value().ub};
        write_edge_mapping_svg(G, H, edgemap, svg_info, nodemap_output_file);
    } else {
        assert(nodemap["G_id"] == h and nodemap["H_id"] == g);
        EdgeMap edgemap(nodemap, H, G);
        SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, h, g, gurobi_params, dist_result.value().ub};
        write_edge_mapping_svg(H, G, edgemap, svg_info, nodemap_output_file);
    }             

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    std::cout << "Overall computation took " << (double)dur.count() / 1000.0 << " [s].\n";
    global_log << "Overall computation took " << (double)dur.count() / 1000.0 << " [s].\n";            

    close_global_log();

    return EXIT_SUCCESS;
}