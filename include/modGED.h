#ifndef MODGED_H
#define MODGED_H

#include "geometric_graph.h"
#include "geometry_helpers.h"
#include "io/config.h"
#include "io/nodemap_io.h"

#include <gurobi_c++.h>
#include <memory>
#include <mutex>
#include <optional>

#include "../external/json/json.hpp"

struct GurobiParams {
    int timelimit_sec;
    bool enforce_cyclic_order;
    bool consider_angle_objective;

    AngleType angle_type;    
    double cost_factor_deletion_insertion;
    double cost_factor_length_difference;
    double cost_factor_angle_difference;

    static GurobiParams finalize(const GurobiConfig &cfg, double angle_scaling_factor) {
        return {
            .timelimit_sec = cfg.timelimit_sec,
            .enforce_cyclic_order = cfg.enforce_cyclic_order,
            .consider_angle_objective = cfg.consider_angle_objective,
            .angle_type = cfg.angle_type,
            .cost_factor_deletion_insertion = cfg.cost_factor_deletion_insertion,
            .cost_factor_length_difference = cfg.cost_factor_length_difference,
            .cost_factor_angle_difference = cfg.cost_factor_angle_difference * angle_scaling_factor
        };
    }
    static GurobiParams from_config(const GurobiConfig &cfg) {
        return finalize (cfg, 1.0);
    }

    void log(std::ostream& out) const {
        out << "\nGurobi Parameters\n";
        out << "  timelimit_sec                  = "
            << timelimit_sec << '\n';
        out << "  enforce_cyclic_order           = "
            << enforce_cyclic_order << '\n';
        out << "  consider_angle_objective       = "
            << consider_angle_objective << '\n';
        out << "  cost_factor_deletion_insertion = "
            << cost_factor_deletion_insertion << '\n';
        out << "  cost_factor_length_difference  = "
            << cost_factor_length_difference << '\n';
        out << "  cost_factor_angle_difference   = "
            << cost_factor_angle_difference << '\n';
        out << "  angle_type                     = "
            << angleTypeToStr(angle_type) << "\n\n";
    }
};



// Result of modGED computations (return value of Gurobi) may vary depending on solver behaviour

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
};

// Default is never returned by modGED computations. This is an option to verify that Gurobi ran at all.
struct Default {};

// Return type for modGED computations. 
using DistanceResult = std::variant<ExactDistance, IntervalDistance, CutoffHit, Infeasible, SolverError, SolverException, Default>;

// This function evaluates whether modGED computation returned a value type (ExactDistance or IntervalDistance), and if so, returns it. 
// Otherwise, nullopt is returned and for Infeasible, SolverError, SolverException and Default an error message is printed.
std::optional<IntervalDistance> evaluate_distance_result(DistanceResult const &res);


//manage one global Gurobi environment
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

// The class modGEDForiSolver implements the ILP formulation FORI, as described in the paper 
// "Enhancing Graph Edit Distance Computation: Stronger and Orientation-based ILP Formulations"
// by Andrea D'Ascenzo, Julian Meffert, Petra Mutzel and Fabrizio Rossi (2025).
// The edge edit costs are given by edge length differences. 
// There is an option to consider angle differences of mapped edges in the objective.
// There are options to enforce the cyclic order given in the GeometricGraph.
// Options are set in the config.
class modGEDForiSolver {
public:
    modGEDForiSolver(GeometricGraph const &G_, 
                     GeometricGraph const &H_, 
                     GurobiParams const &gurobi_params,
                     std::optional<double> cutoff = std::nullopt);

    DistanceResult run_optimization();
    NodeMap return_node_map();
    nlohmann::json return_node_map_json();
    void print_objective_breakdown();

    struct SolverObjectives {
        double total;
        double edge_length_obj;
        double angle_obj;        
    };
    SolverObjectives return_solver_objectives();
    
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
    const GurobiParams &params;

    const GeometricGraph &G;
    const GeometricGraph &H;

    const int nh;
    const int ng;
    const int eh;
    const int eg;

    GRBEnv &env;
    GRBModel model;
    std::chrono::milliseconds optimization_duration;

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

#endif // MODGED_H