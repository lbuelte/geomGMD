#ifndef GEOMGMD_DISTANCE_GEOMGMD_H
#define GEOMGMD_DISTANCE_GEOMGMD_H

// Entry point for computing geomGMD between two geometric graphs.
//
// Usage:
//     GeomGMDResult r = compute_geomGMD(G, H, model, settings);
//     if (r.distance) { use r.distance->ub, r.edge_map, ... }
//     else            { distance_result_error_message(r.result) }

#include "distance/geomGMDSolver.h"
#include "distance/parameters.h"
#include "utility/edgemap.h"
#include "utility/geometric_graph.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

struct GeomGMDOptions {
    // Stop as soon as the lower bound exceeds this value (result: CutoffHit), e.g. used by CLARA.
    std::optional<double> cutoff;
    // Capture Gurobi's own log of this computation (returned in GeomGMDResult::gurobi_log).
    bool capture_gurobi_log = false;
};

struct GeomGMDResult {
    DistanceResult result;                     // outcome of the solver (optimal, interval, cutoff, error, ...)
    std::optional<IntervalDistance> distance;  // = evaluate_distance_result(result); empty if no distance
    EdgeMap edge_map;                          // constrained edge map of the solution (only if distance is set);
                                               // refers to the solver's G and H (see edge_map.g_graph_id/h_graph_id)
    SolveStatistics statistics;                // Gurobi status, MIP gap, node count
    std::vector<std::string> gurobi_log;       // only with GeomGMDOptions::capture_gurobi_log
    std::chrono::milliseconds runtime{0};      // time for building and solving the model
    GeomGMDForiSolver::InstancePairInfo instance;  // polygon ids and sizes of the solver's G and H
};

// Computes geomGMD between G and H. G and H only need to exist during the call.
GeomGMDResult compute_geomGMD(GeometricGraph const &G,
                              GeometricGraph const &H,
                              ModelParameters const &model,
                              SolverSettings const &settings,
                              GeomGMDOptions const &options = {});

#endif // GEOMGMD_DISTANCE_GEOMGMD_H
