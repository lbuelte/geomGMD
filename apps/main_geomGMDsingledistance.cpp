#include <iostream>
#include <fstream>
#include <chrono>
#include <cassert>
#include <filesystem>
#include <sstream>

#include "io/command_line.h"
#include "io/config.h"
#include "io/edgemap_io.h"
#include "io/solver_messages.h"

#include "geomGMDSolver.h"
#include "utility/log.h"


namespace logger = geomgmd::log;

int main(int argc, char* argv[]) 
{  
    auto const command_line = parse_command_line(argc, argv);
    auto const &args = command_line.positional;
    if (args.size() < 2){
        throw std::runtime_error("Error: Need graph IDs as input arguments");
    }
    GraphID g,h;
    try {
        g = std::stoi(args[0]);
    } catch (const std::exception& e) {
        logger::error("Invalid integer: ", args[0]);
    }
    try {
        h = std::stoi(args[1]);
    } catch (const std::exception& e) {
        logger::error("Invalid integer: ", args[1]);
    }    

    std::string config_path = "config_dist.toml";
    if (args.size() >= 3) {
        config_path = args[2];
    }
    auto const config = load_dist_config(config_path);

    std::filesystem::path output_path = 
            std::filesystem::path(config.io.output_path)
            / config.io.dataset
            / config.io.run_name;
    std::filesystem::create_directories(output_path);

    // Console only: a log file in the output folder would overwrite the run.log of a full dataset computation
    configure_logging(resolve_logging(config.logging, command_line), std::nullopt);
    logger::info(config_to_string(config));

    const GurobiParams gurobi_params = GurobiParams::from_config(config.gurobi);
    std::ostringstream gurobi_params_text;
    gurobi_params.log(gurobi_params_text);
    logger::info(gurobi_params_text.str());

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
    
    GeomGMDForiSolver solver(geometricgraphs[g], geometricgraphs[h], gurobi_params);
    auto const res = solver.run_optimization();
    auto dist_result = evaluate_distance_result(res);

    if (!dist_result.has_value()) {
        report_solver_failure(res);
        throw std::runtime_error ("Error! Distance between graphs " + std::to_string(g) +
                                  " and " + std::to_string(h) + " could not be computed!");
    }
    std::filesystem::path edgemap_output_file = 
            output_path 
            / ("edgemap_" + std::to_string(H.polygon_id()) + "_" + std::to_string(G.polygon_id()) + ".svg");
    
    auto edgemap_json = solver.return_edge_map_json();
    solver.print_objective_breakdown();
    bool is_optimal_mapping = edgemap_json["optimal"];
    if (H <= G){
        assert(edgemap_json["G_id"] == g and edgemap_json["H_id"] == h);
        EdgeMap edgemap = EdgeMap::from_json(edgemap_json, G, H);
        SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, g, h, gurobi_params, dist_result.value().ub};
        write_edge_mapping_svg(G, H, edgemap, svg_info, edgemap_output_file);
    } else {
        assert(edgemap_json["G_id"] == h and edgemap_json["H_id"] == g);
        EdgeMap edgemap = EdgeMap::from_json(edgemap_json, H, G);
        SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, h, g, gurobi_params, dist_result.value().ub};
        write_edge_mapping_svg(H, G, edgemap, svg_info, edgemap_output_file);
    }             

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    logger::info("Overall computation took ", (double)dur.count() / 1000.0, " [s].");

    logger::close_file();

    return EXIT_SUCCESS;
}