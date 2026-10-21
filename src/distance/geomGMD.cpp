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
    return r;
}
