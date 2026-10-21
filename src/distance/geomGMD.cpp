#include "distance/geomGMD.h"

GeomGMDResult compute_geomGMD(GeometricGraph const &G,
                              GeometricGraph const &H,
                              ModelParameters const &model,
                              SolverSettings const &settings,
                              GeomGMDOptions const &options)
{
    GeomGMDForiSolver solver(G, H, model, settings, options.cutoff);
    if (options.capture_gurobi_log) {
        solver.capture_gurobi_log();
    }

    GeomGMDResult r;
    r.result = solver.run_optimization();
    r.distance = evaluate_distance_result(r.result);
    if (r.distance.has_value()) {
        r.edge_map = solver.return_edge_map();
    }
    r.statistics = solver.solve_statistics();
    r.gurobi_log = solver.gurobi_log();
    r.runtime = std::chrono::milliseconds(solver.return_runtime_in_ms());
    r.instance = solver.return_run_info();

    // The solver may exchange G and H internally (graph with more edges first); report in the caller's order
    if (solver.graphs_swapped()) {
        r.edge_map = transposed(r.edge_map);
        auto const &i = r.instance;
        r.instance = {.g = i.h, .h = i.g, .ng = i.nh, .nh = i.ng, .eg = i.eh, .eh = i.eg};
    }
    return r;
}
