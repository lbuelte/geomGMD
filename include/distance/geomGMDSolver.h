#ifndef GEOMGMD_DISTANCE_GEOMGMDSOLVER_H
#define GEOMGMD_DISTANCE_GEOMGMDSOLVER_H

#include "utility/geometric_graph.h"
#include "utility/edgemap.h"
#include "distance/parameters.h"

#include <gurobi_c++.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <variant>

// Result of geomGMD computations (return value of Gurobi) may vary depending on solver behaviour

// ExactDistance is returned when Gurobi finds an optimum solution
struct ExactDistance {
    double value;
};

// IntervalDistance is returned when e.g. Gurobi runs into a timeout. 
// It then returns the value of the currently best found solution (ub) and the current best lower bound (lb).
// Often, ub is the value of an optimum solution, but if the gap between lb and ub is > 0, 
// then Gurobi could not verify that this is the case. 
// The idea is to use ub as if it was the best solution.
struct IntervalDistance {
    double lb;
    double ub;

    IntervalDistance(double lb_, double ub_)
    : lb(lb_)
    , ub(ub_)
    {}

    IntervalDistance(ExactDistance dist)
    : lb(dist.value)
    , ub(dist.value)
    {}
};

// An interval distance is smaller when it has smaller ub. lb is used as a tie breaker only.
inline bool operator<(const IntervalDistance& a,
                      const IntervalDistance& b)
{
    constexpr double EPS = 1e-9;
    if (a.ub < b.ub - EPS) return true;
    if (a.ub > b.ub + EPS) return false;
    return a.lb < b.lb - EPS;
}

inline bool operator==(const IntervalDistance& a,
                       const IntervalDistance& b)
{
    constexpr double EPS = 1e-9;
    if (a.ub == b.ub - EPS and a.lb == b.lb) return true;
    return false;
}

// If a cutoff parameter was specified, and during Gurobi, the best lower bound exceeds the cutoff parameter, 
// then CutoffHit is returned (containing said best lower bound)
struct CutoffHit {
    double lb;
};

// If the model becomes infeasible, Infeasible is returned.
struct Infeasible {};

// An onknown status gets returned as a SolverError
struct SolverError {
    int status;
};

// SolverException is returned if Gurobi catches an exception.
struct SolverException {
    int error_code;
    std::string message;
};

// Default is never returned by geomGMD computations. This is an option to verify that Gurobi ran at all.
struct Default {};

// Return type for geomGMD computations. 
using DistanceResult = std::variant<ExactDistance, IntervalDistance, CutoffHit, Infeasible, SolverError, SolverException, Default>;

// This function evaluates whether geomGMD computation returned a value type (ExactDistance or IntervalDistance), and if so, returns it. 
// Otherwise, nullopt is returned. Nothing is printed; see distance_result_error_message.
std::optional<IntervalDistance> evaluate_distance_result(DistanceResult const &res);

// Returns an error message for Infeasible, SolverError and SolverException, and an empty string otherwise.
std::string distance_result_error_message(DistanceResult const &res);


//manage one global Gurobi environment
// One Gurobi environment shared by all solvers (also across threads), created once with OutputFlag = 0.
// Gurobi recommends one environment per thread; a shared environment avoids the start-up cost of an
// environment (and licence checks) for many short computations and has worked in practice. The shared
// environment must not be modified after its creation (models copy its parameters when they are created).
class GurobiEnvManager {
public:
    static GRBEnv& getEnv() {
        static std::once_flag flag;
        static std::unique_ptr<GRBEnv> envPtr;

        std::call_once(flag, []() {
            // Initialize once globally
            envPtr = std::make_unique<GRBEnv>(true); // Create but don't start logging yet
            envPtr->set("OutputFlag", "0");
            envPtr->start(); // Start the environment
        });

        return *envPtr;
    }

private:
    GurobiEnvManager() = default;
};

struct DirectedPathLength2 {
    VertexID v1;
    VertexID v2; 
    VertexID v3;
    double angle = 0.0;
};

// The class GeomGMDForiSolver implements the ILP formulation FORI, as described in the paper 
// "Enhancing Graph Edit Distance Computation: Stronger and Orientation-based ILP Formulations"
// by Andrea D'Ascenzo, Julian Meffert, Petra Mutzel and Fabrizio Rossi (2025).
// The edge edit costs are given by edge length differences. 
// There is an option to consider angle differences of mapped edges in the objective.
// There are options to enforce the cyclic order given in the GeometricGraph.
// Options are set in the config.
// Statistics of the last Gurobi run (see GeomGMDForiSolver::solve_statistics).
struct SolveStatistics {
    std::optional<int> status;      // Gurobi status code (GRB_OPTIMAL, GRB_TIME_LIMIT, ...); empty if not optimized
    std::optional<double> mip_gap;  // final relative MIP gap; empty if no solution was found
    double node_count = 0;          // number of explored branch-and-bound nodes

    bool is_optimal() const { return status.has_value() and *status == GRB_OPTIMAL; }
};

// Name of a Gurobi status code, e.g. "OPTIMAL", "TIME_LIMIT"; "UNKNOWN_<code>" for unknown codes.
std::string gurobi_status_name(int status);

// Collects Gurobi's log lines via a message callback (see GeomGMDForiSolver::capture_gurobi_log).
class GurobiLogCapture : public GRBCallback {
public:
    std::vector<std::string> lines;
protected:
    void callback() override;
};

class GeomGMDForiSolver {
public:
    // G_ and H_ are not copied: they must outlive the solver.
    GeomGMDForiSolver(GeometricGraph const &G_,
                     GeometricGraph const &H_,
                     ModelParameters const &model_parameters,
                     SolverSettings const &solver_settings,
                     std::optional<double> cutoff = std::nullopt);

    // Temporary graphs would be destroyed while the solver still refers to them: compile error.
    GeomGMDForiSolver(GeometricGraph &&, GeometricGraph const &, ModelParameters const &,
                      SolverSettings const &, std::optional<double> = std::nullopt) = delete;
    GeomGMDForiSolver(GeometricGraph const &, GeometricGraph &&, ModelParameters const &,
                      SolverSettings const &, std::optional<double> = std::nullopt) = delete;
    GeomGMDForiSolver(GeometricGraph &&, GeometricGraph &&, ModelParameters const &,
                      SolverSettings const &, std::optional<double> = std::nullopt) = delete;

    // Captures Gurobi's own log of this computation (for debugging; call before run_optimization).
    // The log is not printed; the captured lines are returned by gurobi_log().
    // Without this call, Gurobi's output stays switched off and no callback is registered.
    void capture_gurobi_log();
    std::vector<std::string> gurobi_log() const;

    DistanceResult run_optimization();
    // Status, MIP gap and node count of the last run_optimization() (only read from Gurobi).
    SolveStatistics solve_statistics() const { return statistics; }
    // Constrained edge map of the computed solution, between the solver's G and H (see g_graph_id/h_graph_id;
    // the solver uses the graph with more edges as G). Empty if no solution was computed.
    EdgeMap return_edge_map();

    struct InstancePairInfo {
        PolygonID g, h;
        VertexID ng, nh;
        EdgeID eg, eh;
    };
    InstancePairInfo return_run_info() const {
        return {
            .g  = G.polygon_id(),
            .h  = H.polygon_id(),
            .ng = G.num_vertices(),
            .nh = H.num_vertices(),
            .eg = G.num_edges(),
            .eh = H.num_edges()
        };
    }
    int return_runtime_in_ms() const {
        return (int)optimization_duration.count();
    }

private:
    const ModelParameters params;
    const SolverSettings settings;

    // References to the caller's graphs (G = the one with more edges, see constructor); not copied
    const GeometricGraph &G;
    const GeometricGraph &H;

    const int nh;
    const int ng;
    const int eh;
    const int eg;

    GRBEnv &env;
    GRBModel model;
    std::unique_ptr<GurobiLogCapture> log_capture;
    std::chrono::milliseconds optimization_duration;
    SolveStatistics statistics;

    const bool use_path_variables;

    std::vector<std::vector<GRBVar>> x; // node map variables
    std::vector<std::vector<GRBVar>> z; // edge map variables
    std::vector<std::vector<GRBVar>> p; // path map variables (for length2-paths)

    std::vector<std::vector<int>> node_pair_to_zidx_G;
    std::vector<std::vector<int>> node_pair_to_zidx_H;

    std::vector<DirectedPathLength2> directed_paths_G;
    std::vector<DirectedPathLength2> directed_paths_H;

    void create_node_and_edge_variables();
    void create_path_variables();
    void add_node_mapping_constraints();
    void add_node_edge_linking_constraints();
    void add_edge_path_linking_constraints();
    void add_cyclic_order_constraints();
};

#endif // GEOMGMD_DISTANCE_GEOMGMDSOLVER_H