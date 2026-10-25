#include <iostream>
#include <fstream>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <sstream>

#include "io/command_line.h"
#include "io/config.h"
#include "io/edgemap_io.h"
#include "io/graph_io.h"
#include "io/run_setup.h"
#include "distance/geomGMD.h"
#include "distance/mapping_cost.h"
#include "io/edgemap_svg.h"
#include "io/solver_messages.h"

#include "distance/geomGMDSolver.h"
#include "utility/log.h"


namespace logger = geomgmd::log;

namespace {

// Graph id = position in the input file (non-negative integer)
GraphID parse_graph_id(std::string const &s)
{
    size_t pos = 0;
    GraphID id = 0;
    bool valid = not s.empty() and std::isdigit(static_cast<unsigned char>(s[0]));
    if (valid) {
        try {
            id = std::stoull(s, &pos);
        } catch (std::exception const &) {
            valid = false;
        }
    }
    if (not valid or pos != s.size()) {
        throw std::runtime_error("Invalid graph id: " + s + " (expected a non-negative integer)");
    }
    return id;
}

int run(int argc, char* argv[]) 
{  
    auto const command_line = parse_command_line(argc, argv);
    auto const &args = command_line.positional;
    if (args.size() < 2){
        throw std::runtime_error("Usage: " + std::string(argv[0]) + " <graph id> <graph id> [config file] [options]");
    }
    GraphID const g = parse_graph_id(args[0]);
    GraphID const h = parse_graph_id(args[1]);

    std::string config_path = "config_dist.toml";
    if (args.size() >= 3) {
        config_path = args[2];
    }
    auto config = load_dist_config(config_path);

    // Own subfolder of the run folder, so that the files of a full dataset computation with the same
    // run_name are not touched
    std::filesystem::path output_path = run_output_path(config.io) / "pair";
    std::filesystem::create_directories(output_path);

    // Console only: a log file in the output folder would overwrite the run.log of a full dataset computation
    config.logging = resolve_logging(config.logging, command_line);
    configure_logging(config.logging, std::nullopt);
    for (auto const &warning : config.warnings) logger::warning(warning);
    logger::info(config);

    // Log config in extra file
    copy_config(config_path, output_path, "config_" + std::to_string(g) + "_" + std::to_string(h) + ".toml");

    // Reading input graphs
    std::filesystem::path json_path = graphs_json_path(config.io);
    std::vector<GeometricGraph> geometricgraphs;
    read_graph_vector_from_json(geometricgraphs, json_path.string());

    for (GraphID id : {g, h}) {
        if (id >= geometricgraphs.size()) {
            throw std::runtime_error("Graph id " + std::to_string(id) + " is not in the dataset ("
                                     + std::to_string(geometricgraphs.size()) + " graphs, ids 0 to "
                                     + std::to_string(geometricgraphs.size() - 1) + ")");
        }
    }
    GeometricGraph &G = geometricgraphs[g];
    GeometricGraph &H = geometricgraphs[h];

    auto begin = std::chrono::high_resolution_clock::now();
    
    auto const r = compute_geomGMD(geometricgraphs[g], geometricgraphs[h], config.model, config.solver,
                                   {.cutoff = std::nullopt, .capture_gurobi_log = config.logging.gurobi_log});
    report_gurobi_log(config.logging, geometricgraphs[g], geometricgraphs[h], r);
    auto dist_result = r.distance;

    if (!dist_result.has_value()) {
        report_solver_failure(r.result);
        throw std::runtime_error ("Error! Distance between graphs " + std::to_string(g) +
                                  " and " + std::to_string(h) + " could not be computed!");
    }
    std::filesystem::path edgemap_output_file = 
            output_path 
            / ("edgemap_" + std::to_string(g) + "_" + std::to_string(h) + ".svg");
    
    auto mapping_json = edgemap_json(r);
    {
        // Cost breakdown of the edge map
        auto const cost = compute_mapping_cost(G, H, r.edge_map, config.model);
        logger::info("Cost of the edge map: deletion ", cost.deletion, ", insertion ", cost.insertion,
                     ", length difference ", cost.length_difference, ", path angles ", cost.path_angle,
                     ", total ", cost.total());
    }
    bool is_optimal_mapping = mapping_json["optimal"];
    SVGLegendInfo svg_info{is_optimal_mapping, config.io.dataset, g, h, config.model, dist_result.value().ub};
    write_edge_mapping_svg(G, H, r.edge_map, svg_info, edgemap_output_file);

    auto end = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    logger::info("Overall computation took ", (double)dur.count() / 1000.0, " [s].");

    logger::close_file();

    return EXIT_SUCCESS;
}

} // end of anonymous namespace


int main(int argc, char* argv[])
{
    return run_main(run, argc, argv);
}
