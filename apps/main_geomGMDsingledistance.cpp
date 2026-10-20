#include <iostream>
#include <fstream>
#include <chrono>
#include <cassert>
#include <filesystem>
#include <sstream>

#include "io/command_line.h"
#include "io/config.h"
#include "io/edgemap_io.h"
#include "io/edgemap_svg.h"
#include "io/solver_messages.h"

#include "distance/geomGMDSolver.h"
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
    auto config = load_dist_config(config_path);

    std::filesystem::path output_path = 
            std::filesystem::path(config.io.output_path)
            / config.io.dataset
            / config.io.run_name;
    std::filesystem::create_directories(output_path);

    // Console only: a log file in the output folder would overwrite the run.log of a full dataset computation
    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, std::nullopt);
    logger::info(config_to_string(config));

    logger::info(to_string(config.model, config.solver));

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
    
    GeomGMDForiSolver solver(geometricgraphs[g], geometricgraphs[h], config.model, config.solver);
    if (config.logging.gurobi_log) solver.capture_gurobi_log();
    auto const res = solver.run_optimization();
    report_gurobi_log(config.logging, geometricgraphs[g], geometricgraphs[h], res, solver.gurobi_log());
    auto dist_result = evaluate_distance_result(res);

    if (!dist_result.has_value()) {
        report_solver_failure(res);
        throw std::runtime_error ("Error! Distance between graphs " + std::to_string(g) +
                                  " and " + std::to_string(h) + " could not be computed!");
    }
    std::filesystem::path edgemap_output_file = 
            output_path 
            / ("edgemap_" + std::to_string(H.polygon_id()) + "_" + std::to_string(G.polygon_id()) + ".svg");
    
    auto mapping_json = edgemap_json(solver);
    solver.print_objective_breakdown();
    bool is_optimal_mapping = mapping_json["optimal"];
    if (H <= G){
        assert(mapping_json["G_id"] == g and mapping_json["H_id"] == h);
        EdgeMap edgemap = edgemap_from_json(mapping_json);
        SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, g, h, config.model, dist_result.value().ub};
        write_edge_mapping_svg(G, H, edgemap, svg_info, edgemap_output_file);
    } else {
        assert(mapping_json["G_id"] == h and mapping_json["H_id"] == g);
        EdgeMap edgemap = edgemap_from_json(mapping_json);
        SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, h, g, config.model, dist_result.value().ub};
        write_edge_mapping_svg(H, G, edgemap, svg_info, edgemap_output_file);
    }             

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    logger::info("Overall computation took ", (double)dur.count() / 1000.0, " [s].");

    logger::close_file();

    return EXIT_SUCCESS;
}