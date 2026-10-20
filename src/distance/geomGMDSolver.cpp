#include "distance/geomGMDSolver.h"

#include "utility/angle_computation.h"
#include "utility/overloaded.h"

#include <stdexcept>

namespace{

// Finds all directed paths of length 2 in the graph g, i.e., all triples of vertices (i, j, h)
// such that (i, j) and (j, h) are edges in g. The paths ijh and hji are considered different.
// Further, the function computes the signed or unsigned angle between the two edges (i, j) and (j, h) 
// for each path and returns it together with the paths.
std::vector<DirectedPathLength2> findAllPathsOfLength2(GeometricGraph const &G, AngleType const &angle_type) {
    std::vector<DirectedPathLength2> paths;
    for (auto j: G.vertices()) {
        for (VertexID i: G.neighbors(j.id)) {
            if (i == j.id) continue;
            for (VertexID h: G.neighbors(j.id)) {
                if (h == j.id || h == i) continue;
                auto const angle = (angle_type == AngleType::Signed ?
                        compute_signed_angle_between_lines(G.vertex(i).p, j.p, G.vertex(h).p) :
                        compute_unsigned_angle_between_lines(G.vertex(i).p, j.p, G.vertex(h).p));
                paths.emplace_back(i, j.id, h, angle);
            }
        }
    }
    return paths;
}


// Returns the sum of the variables in column 'index' of the 2D vector 'vars'.
GRBLinExpr colSum(const std::vector<std::vector<GRBVar>> &vars, int index) {
    GRBLinExpr sum = 0;
    for (const auto &vec: vars) sum += vec[index];
    return sum;
}


// Returns the sum of the variables in the vector 'vec'.
GRBLinExpr vecSum(const std::vector<GRBVar> &vec) {
    GRBLinExpr sum = 0;
    for (const auto &val: vec) sum += val;
    return sum;
}


// Computes the constant cost of deleting all edges of G and inserting all edges of H.
long double compute_gamma(const GeometricGraph &G, const GeometricGraph &H, double c_ins_del) {
    double gamma = 0;
    for (auto e: G.edges()) {
        gamma += e.length * c_ins_del;
    }
    for (auto e: H.edges()) {
        gamma += e.length * c_ins_del;
    }
    return gamma;
} 

// Helper function to compute the edge length difference objective for FORI using GeometricGraph
GRBLinExpr compute_edge_length_objective_FORI(GeometricGraph const &G, GeometricGraph const &H, 
        GurobiParams const &params, const std::vector<std::vector<GRBVar>> &z) {

    GRBLinExpr costsum = 0;
    // Add the cost of mapping directed edge ij in G to directed edges kl and lk in H
    // and subtract the cost of deleting directed edge ij and inserting directed edges kl and lk for all edge pairs.
    int ij = 0, kl;
    for (const auto &edgeG: G.edges()) {
        kl = 0;
        for (const auto &edgeH: H.edges()) {
            double cost = (std::abs(edgeG.length - edgeH.length) * params.cost_factor_length_difference
              - (edgeG.length + edgeH.length) * params.cost_factor_deletion_insertion);

            costsum += cost * z[ij][kl];
            costsum += cost * z[ij][kl+1];
            
            kl += 2;
        }
        ij++;
    }
    return costsum;
}

// Helper function to compute the angle difference objective
GRBLinExpr compute_angle_diff_objective(GurobiParams const &params,
        std::vector<DirectedPathLength2> paths2G, std::vector<DirectedPathLength2> paths2H, std::vector<std::vector<GRBVar>> p) {

    GRBLinExpr costsum = 0;
    // Add the cost of mapping path ijh in G to path klm in H for all path pairs.
    int ijh = 0, klm;
    for (const auto &pG: paths2G) {
        auto [i, j, h, alphaijh] = pG;
        klm = 0;
        for (const auto &pH: paths2H) {
            auto [k, l, m, alphaklm] = pH;
            // The cost of mapping path ijh in G to path klm in H is given by 
            // the difference of their signed angles, weighted by the constant c_angle_diff.
            costsum += p[ijh][klm] * std::abs(alphaijh - alphaklm) * params.cost_factor_angle_difference;
            klm++;
        }
        ijh++;
    }
    return costsum;
}


// Computes the linear expression representing the objective cost based on the edge mapping variables z
// and the path mapping variables p. Here, the edges of G are directed and the for the edges of H we consider both directions. 
// The variables z[ij][kl] represent the mapping of directed edge ij in G to the directed edge kl in H and the variables z[ij][kl+1]
// represent the mapping of directed edge ij in G to the directed edge lk in H.
GRBLinExpr compute_objective_cost_FORI(GeometricGraph const &G, GeometricGraph const &H, 
        GurobiParams const &params, const std::vector<std::vector<GRBVar>> &z, 
        std::vector<DirectedPathLength2> paths2G, std::vector<DirectedPathLength2> paths2H, std::vector<std::vector<GRBVar>> p) 
{
    return compute_edge_length_objective_FORI(G, H, params, z) + compute_angle_diff_objective(params, paths2G, paths2H, p);
}


// Print the solution of the ILP model, i.e., all variables with non-zero value.
void print_solution(GRBModel const &model, std::vector<std::vector<GRBVar>> const &x, 
        std::vector<std::vector<GRBVar>> const &y, std::vector<std::vector<GRBVar>> const &p, double gamma){
    std::cout << "Optimum geomGMD value: " << model.get(GRB_DoubleAttr_ObjVal) + gamma <<" with objective value " 
                    << model.get(GRB_DoubleAttr_ObjVal) << " and constant " << gamma << std::endl;
    auto n_g = x.size();
    auto n_h = x.front().size();
    auto m_g = y.size();
    auto m_h = y.front().size();
    auto p_g = p.size();
    auto p_h = p.front().size();

    for (int i = 0; i < n_g; i++) {
        for (int k = 0; k < n_h; k++) {
            if (x[i][k].get(GRB_DoubleAttr_X) > 0){
                std::cout << x[i][k].get(GRB_StringAttr_VarName) << " = " << x[i][k].get(GRB_DoubleAttr_X) << std::endl;
            }
        }
    }
    for (int ij = 0; ij < m_g; ij++) {
        for (int kl = 0; kl < m_h; kl++) {
            if (y[ij][kl].get(GRB_DoubleAttr_X) > 0){
                std::cout << y[ij][kl].get(GRB_StringAttr_VarName) << " = " << y[ij][kl].get(GRB_DoubleAttr_X) << std::endl;
            }
        }
    }
    for (int ijh = 0; ijh < p_g; ijh++) {
        for (int klm = 0; klm < p_h; klm++) {
            if (p[ijh][klm].get(GRB_DoubleAttr_X) > 0){
                std::cout << p[ijh][klm].get(GRB_StringAttr_VarName) << " = " << p[ijh][klm].get(GRB_DoubleAttr_X) << std::endl;
            }
        }
    }
}

// Since the paths do not care about orientation, we have to make sure that we get the edge in G in the right direction.
auto get_directed_edge_G(auto const i, auto const j, std::vector<std::vector<int>> const &node_pair_to_zidx_G){
    if (node_pair_to_zidx_G[i][j] != -1){
        return node_pair_to_zidx_G[i][j];
    }
    else if (node_pair_to_zidx_G[j][i] != -1){
        return node_pair_to_zidx_G[j][i];
    }
    else {
        std::cerr << "Error: For j adjacent to i, we stored neither the edge ij nor the edge ji." << std::endl;
        return -1;
    }       
}

} // end of anonymous namespace


std::optional<IntervalDistance> evaluate_distance_result(const DistanceResult& res)
{
    return std::visit(overloaded{
        [](const ExactDistance& d) -> std::optional<IntervalDistance> {
            return IntervalDistance(d);
        },
        [](const IntervalDistance& d) -> std::optional<IntervalDistance> {
            return d;
        },
        [](const Default&) -> std::optional<IntervalDistance> {
            return std::nullopt;
        },
        [](const CutoffHit&) -> std::optional<IntervalDistance> {
            return std::nullopt;
        },
        [](const Infeasible&) -> std::optional<IntervalDistance> {
            return std::nullopt;
        },
        [](const SolverError&) -> std::optional<IntervalDistance> {
            return std::nullopt;
        },
        [](const SolverException&) -> std::optional<IntervalDistance> {
            return std::nullopt;
        }
    }, res);
}

std::string distance_result_error_message(const DistanceResult& res)
{
    return std::visit(overloaded{
        [](const Infeasible&) -> std::string {
            return "Error: Gurobi finished with status Infeasible!";
        },
        [](const SolverError& e) -> std::string {
            return "Error: Gurobi finished with status " + std::to_string(e.status) + "!";
        },
        [](const SolverException& e) -> std::string {
            return "Error: Gurobi ran into an exception! Error code = " + std::to_string(e.error_code)
                   + ": " + e.message;
        },
        [](const auto&) -> std::string {
            return "";
        }
    }, res);
}

GeomGMDForiSolver::GeomGMDForiSolver(GeometricGraph const &G_, 
                                    GeometricGraph const &H_, 
                                    GurobiParams const &gurobi_params,
                                    std::optional<double> cutoff)
    : params(gurobi_params)
    , use_path_variables(params.consider_angle_objective or params.enforce_cyclic_order)
    , G(std::max(G_, H_))
    , H(std::min(H_, G_))
    , nh(H.num_vertices())
    , ng(G.num_vertices())
    , eh(H.num_edges())
    , eg(G.num_edges())
    , env(GurobiEnvManager::getEnv())
    , model(env)
{
    env.set("OutputFlag", "0");
    model.set(GRB_StringAttr_ModelName,  "geomGMD_FORI");
    model.set(GRB_IntParam_Threads, 1);
    model.set(GRB_DoubleParam_TimeLimit, params.timelimit_sec);
    model.set(GRB_IntParam_Seed, 1);
    model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);
    model.set(GRB_DoubleParam_Cutoff, GRB_INFINITY);
    if (cutoff.has_value()) {
        model.set(GRB_DoubleParam_Cutoff, cutoff.value()); 
    }
    if (use_path_variables){
        directed_paths_G = findAllPathsOfLength2(G, params.angle_type);
        directed_paths_H = findAllPathsOfLength2(H, params.angle_type);
    }
}

std::string gurobi_status_name(int status)
{
    switch (status) {
        case GRB_LOADED:          return "LOADED";
        case GRB_OPTIMAL:         return "OPTIMAL";
        case GRB_INFEASIBLE:      return "INFEASIBLE";
        case GRB_INF_OR_UNBD:     return "INF_OR_UNBD";
        case GRB_UNBOUNDED:       return "UNBOUNDED";
        case GRB_CUTOFF:          return "CUTOFF";
        case GRB_ITERATION_LIMIT: return "ITERATION_LIMIT";
        case GRB_NODE_LIMIT:      return "NODE_LIMIT";
        case GRB_TIME_LIMIT:      return "TIME_LIMIT";
        case GRB_SOLUTION_LIMIT:  return "SOLUTION_LIMIT";
        case GRB_INTERRUPTED:     return "INTERRUPTED";
        case GRB_NUMERIC:         return "NUMERIC";
        case GRB_SUBOPTIMAL:      return "SUBOPTIMAL";
        case GRB_INPROGRESS:      return "INPROGRESS";
        case GRB_USER_OBJ_LIMIT:  return "USER_OBJ_LIMIT";
        case GRB_WORK_LIMIT:      return "WORK_LIMIT";
        case GRB_MEM_LIMIT:       return "MEM_LIMIT";
    }
    return "UNKNOWN_" + std::to_string(status);
}

void GurobiLogCapture::callback()
{
    if (where != GRB_CB_MESSAGE) {
        return;
    }
    // A message may contain several lines and usually ends with a line break
    std::string message = getStringInfo(GRB_CB_MSG_STRING);
    size_t start = 0;
    while (start < message.size()) {
        size_t end = message.find('\n', start);
        if (end == std::string::npos) {
            end = message.size();
        }
        lines.push_back(message.substr(start, end - start));
        start = end + 1;
    }
}

void GeomGMDForiSolver::capture_gurobi_log()
{
    log_capture = std::make_unique<GurobiLogCapture>();
    // Switch off console output first (silently, while output is still off), then switch on the log
    model.set(GRB_IntParam_LogToConsole, 0);
    model.set(GRB_IntParam_OutputFlag, 1);
    model.setCallback(log_capture.get());
}

std::vector<std::string> GeomGMDForiSolver::gurobi_log() const
{
    return log_capture ? log_capture->lines : std::vector<std::string>();
}

DistanceResult GeomGMDForiSolver::run_optimization()
{
    try{
        auto begin = std::chrono::high_resolution_clock::now();
        // Compute constant deletion/insertion cost
        const double gamma = compute_gamma(G, H, params.cost_factor_deletion_insertion);

        // Add variables and constraints to model
        create_node_and_edge_variables();    
        add_node_mapping_constraints();
        add_node_edge_linking_constraints();
        if (use_path_variables){
            create_path_variables();
            add_edge_path_linking_constraints();        
        }
        if (params.enforce_cyclic_order){
            add_cyclic_order_constraints();
        }

        GRBLinExpr edge_length_obj = compute_edge_length_objective_FORI(G, H, params, z);
        GRBLinExpr angle_obj = compute_angle_diff_objective(params, directed_paths_G, directed_paths_H, p);

        // Set model objective
        if (params.consider_angle_objective){
            model.setObjective(edge_length_obj + gamma + angle_obj, GRB_MINIMIZE);
        } else {
            model.setObjective(edge_length_obj + gamma, GRB_MINIMIZE);
        }

        model.optimize();

        auto end = std::chrono::high_resolution_clock::now();
        optimization_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

        // Handle optimization output
        auto status = model.get(GRB_IntAttr_Status);

        // Record statistics (only reads attributes; errors are ignored so that the result is not influenced)
        statistics.status = status;
        try {
            statistics.node_count = model.get(GRB_DoubleAttr_NodeCount);
            if (model.get(GRB_IntAttr_SolCount) > 0) {
                statistics.mip_gap = model.get(GRB_DoubleAttr_MIPGap);
            }
        } catch (GRBException &) {
        }
        // auto result = [&]() -> DistanceResult {
        if (status == GRB_OPTIMAL) {
            return ExactDistance{ model.get(GRB_DoubleAttr_ObjVal)};
        } 
        else if (status == GRB_CUTOFF) {
            return CutoffHit{ model.get(GRB_DoubleAttr_ObjBound) };
        }
        else if (status == GRB_TIME_LIMIT) {
            return IntervalDistance( model.get(GRB_DoubleAttr_ObjBound), 
                                        model.get(GRB_DoubleAttr_ObjVal) );
        } else if (status == GRB_INFEASIBLE) {
            model.computeIIS();
            model.write("infeasible.ilp");
            return Infeasible{};
        } else if (status == GRB_NUMERIC) {
            if (model.get(GRB_IntAttr_SolCount) > 0) {
                return IntervalDistance( model.get(GRB_DoubleAttr_ObjBound), 
                                            model.get(GRB_DoubleAttr_ObjVal));
            } else {
                return SolverError{ status };
            }        
        }
        return SolverError{ status };    
        // }();

    } catch (GRBException &e) {
        if (e.getErrorCode() == 10009 || e.getErrorCode() == 10032) {
            // Licensing failure: abort process-level computation
            throw std::runtime_error("Gurobi licensing issue (error code " + std::to_string(e.getErrorCode())
                                     + "): " + e.getMessage() + " Aborting program!");
        }     
        return SolverException{ e.getErrorCode(), e.getMessage() };
    }
}

EdgeMap GeomGMDForiSolver::return_edge_map()
{
    if (model.get(GRB_IntAttr_SolCount) == 0){
        // No solution was computed (callers only request the edge map after a successful optimization)
        return EdgeMap(false, std::vector<EdgeMap::EdgePair>(), G, H);
    }

    double tol = model.get(GRB_DoubleParam_IntFeasTol);
    auto isOne = [&tol](double val) {
        return val > 1.0 - tol;
    };

    bool optimal = (model.get(GRB_IntAttr_Status) == GRB_OPTIMAL);
    std::vector<EdgeMap::EdgePair> pairs;
    std::vector<bool> is_mapped(eh, false);
    for (int i = 0; i < eg; i++) {
        bool is_mapped_i = false;
        // z has two columns per edge k in H: 2k for (v1,v2) and 2k+1 for (v2,v1)
        for (int c = 0; c < 2*eh; c++) {
            if (isOne(z[i][c].get(GRB_DoubleAttr_X))){
                EdgeID const k = c / 2;
                is_mapped_i = true;
                is_mapped[k] = true;
                pairs.emplace_back(i, k);
                break;
            }
        }
        if (not is_mapped_i){
            pairs.emplace_back(i, std::nullopt);
        }
    }
    for (int k = 0; k < eh; k++) {
        if (not is_mapped[k]){
            pairs.emplace_back(std::nullopt, k);
        }
    }
    return EdgeMap(optimal, pairs, G, H);
}

nlohmann::json GeomGMDForiSolver::return_edge_map_json()
{
    auto map = return_edge_map();
    return map.to_json(G.graph_id(), H.graph_id());
}

void GeomGMDForiSolver::print_objective_breakdown() {
    GRBLinExpr edge_length_expr = compute_edge_length_objective_FORI(G, H, params, z);
    GRBLinExpr angle_expr = compute_angle_diff_objective(params, directed_paths_G, directed_paths_H, p);
    double gamma_val = compute_gamma(G, H, params.cost_factor_deletion_insertion);

    std::cout << "Gamma (constant):    " << gamma_val << std::endl;
    std::cout << "Edge length obj:     " << edge_length_expr.getValue() + gamma_val << std::endl;
    std::cout << "Angle obj:           " << angle_expr.getValue() << std::endl;
    std::cout << "Total ILP objective: " << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
}

GeomGMDForiSolver::SolverObjectives GeomGMDForiSolver::return_solver_objectives(){
    GRBLinExpr edge_length_expr = compute_edge_length_objective_FORI(G, H, params, z);
    GRBLinExpr angle_expr = compute_angle_diff_objective(params, directed_paths_G, directed_paths_H, p);
    double gamma_val = compute_gamma(G, H, params.cost_factor_deletion_insertion);

    return {model.get(GRB_DoubleAttr_ObjVal), edge_length_expr.getValue() + gamma_val, angle_expr.getValue()};
}

void GeomGMDForiSolver::create_node_and_edge_variables()
{
    // Adding binary node mapping variables with no contribution to objective
    x = std::vector<std::vector<GRBVar>>(ng, std::vector<GRBVar>(nh));
    for (int i = 0; i < ng; i++) {
        for (int k = 0; k < nh; k++) {
            x[i][k] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "x_" + std::to_string(i) + "_" + std::to_string(k));
        }
    }

    node_pair_to_zidx_G = std::vector<std::vector<int>>(ng, std::vector<int>(ng, -1));
    node_pair_to_zidx_H = std::vector<std::vector<int>>(nh, std::vector<int>(nh, -1));

    // Adding binary edge mapping variables, here without contribution to objective.
    // We need 2*eh variables for each edge in G, 
    // and we consecutively add two variables for mapping the edge ij in G to the edges kl and lk in H.
    z = std::vector<std::vector<GRBVar>>(eg, std::vector<GRBVar>(2*eh));
    int ij = 0, kl;
    for (auto edgeG: G.edges()) {
        node_pair_to_zidx_G[edgeG.v1][edgeG.v2] = ij;            
        kl = 0;
        for (auto edgeH: H.edges()) {
            if (ij == 0) {
                node_pair_to_zidx_H[edgeH.v1][edgeH.v2] = kl;
                node_pair_to_zidx_H[edgeH.v2][edgeH.v1] = kl + 1;
            }
            z[ij][kl] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "z_" + std::to_string(edgeG.v1) + "_" + std::to_string(edgeG.v2) 
                                                                + "__" + std::to_string(edgeH.v1) + "_" + std::to_string(edgeH.v2));
            z[ij][kl+1] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "z_" + std::to_string(edgeG.v1) + "_" + std::to_string(edgeG.v2) 
                                                                    + "__" + std::to_string(edgeH.v2) + "_" + std::to_string(edgeH.v1));
            kl += 2;
        }
        ij++;
        }
}

void GeomGMDForiSolver::create_path_variables()
{
    // Adding binary path mapping variables, here without contribution to objective.
    p = std::vector<std::vector<GRBVar>>(directed_paths_G.size(), std::vector<GRBVar>(directed_paths_H.size()));
    for (size_t a = 0; a < directed_paths_G.size(); a++) {
        auto [i, j, h, alphaijh] = directed_paths_G[a];
        for (size_t b = 0; b < directed_paths_H.size(); b++) {
            auto [k, l, m, alphaklm] = directed_paths_H[b];
            p[a][b] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, "p_" + std::to_string(i) + "_" + std::to_string(j) + "_" + std::to_string(h) 
                                                            + "__" + std::to_string(k) + "_" + std::to_string(l) + "_" + std::to_string(m));
        }
    }
}

void GeomGMDForiSolver::add_node_mapping_constraints()
{
    // Add constraints ensuring a valid node mapping, i.e., 
    // each node in G is mapped to at most one node in H and vice versa.
    for (int i = 0; i < ng; i++){
        model.addConstr(vecSum(x[i]) <= 1, "Vertex Mapping G");
    }
    for (int k = 0; k < nh; k++){
        model.addConstr(colSum(x, k) <= 1, "Vertex Mapping H");
    }
}

void GeomGMDForiSolver::add_node_edge_linking_constraints()
{
    // Add topological constraints linking node mapping variables to edge mapping variables.
    for (auto const &edgeG : G.edges()){    
        auto const i = edgeG.v1;
        auto const j = edgeG.v2;    
        for (int k = 0; k < nh; k++){                    
            GRBLinExpr sum = 0;    
            // Outgoing edges            
            for (auto const l : H.neighbors(k)){
                sum += z[node_pair_to_zidx_G[i][j]][node_pair_to_zidx_H[k][l]];
            }
            model.addConstr(sum <= x[i][k], "Outgoing Edge Sum");

            sum = 0;
            // Ingoing edges
            for (auto const l : H.neighbors(k)){
                sum += z[node_pair_to_zidx_G[i][j]][node_pair_to_zidx_H[l][k]];
            }
            model.addConstr(sum <= x[j][k], "Ingoing Edge Sum");              
        }
    }
    // Add constraint for both directions of H-edges
    for (auto const &edgeH : H.edges()){
        for (int dir = 0; dir < 2; ++dir) {

            auto k = (dir == 0) ? edgeH.v1 : edgeH.v2;
            auto l = (dir == 0) ? edgeH.v2 : edgeH.v1;

            for (int i = 0; i < ng; i++){
                GRBLinExpr sum = 0;

                for (auto const j : G.neighbors(i)){
                    if (node_pair_to_zidx_G[i][j] != -1){
                        sum += z[node_pair_to_zidx_G[i][j]][node_pair_to_zidx_H[k][l]];
                    }
                    else if (node_pair_to_zidx_G[j][i] != -1){
                        sum += z[node_pair_to_zidx_G[j][i]][node_pair_to_zidx_H[l][k]];
                    }
                }

                model.addConstr(sum <= x[i][k], "Edge Sum H");
            }
        }
    }
    // These are already implied for an optimal solution, but may help optimization
    for (auto const &edgeG : G.edges()){
        auto const i = edgeG.v1;
        auto const j = edgeG.v2;
        for (auto const &edgeH :H.edges()){
            auto const k = edgeH.v1;
            auto const l = edgeH.v2;
            model.addConstr(z[node_pair_to_zidx_G[i][j]][node_pair_to_zidx_H[k][l]] >= x[i][k] + x[j][l] - 1);
            model.addConstr(z[node_pair_to_zidx_G[i][j]][node_pair_to_zidx_H[l][k]] >= x[i][l] + x[j][k] - 1);
        }
    }
}

void GeomGMDForiSolver::add_edge_path_linking_constraints()
{
    for (size_t a = 0; a < directed_paths_G.size(); a++) {
        auto [i, j, h, alphaijh] = directed_paths_G[a];
        for (size_t b = 0; b < directed_paths_H.size(); b++) {
            auto [k, l, m, alphaklm] = directed_paths_H[b];

            // Constraints enforcing Edge - Path relation
            model.addConstr(p[a][b] 
                            >= z[get_directed_edge_G(i,j,node_pair_to_zidx_G)][node_pair_to_zidx_H[k][l]] + z[get_directed_edge_G(j,h,node_pair_to_zidx_G)][node_pair_to_zidx_H[l][m]]
                             + z[get_directed_edge_G(i,j,node_pair_to_zidx_G)][node_pair_to_zidx_H[l][k]] + z[get_directed_edge_G(j,h,node_pair_to_zidx_G)][node_pair_to_zidx_H[m][l]] - 1,
                        "Path Edge Relation");    
            model.addConstr(p[a][b] <= z[get_directed_edge_G(i,j,node_pair_to_zidx_G)][node_pair_to_zidx_H[k][l]] + z[get_directed_edge_G(i,j,node_pair_to_zidx_G)][node_pair_to_zidx_H[l][k]], "Path UB1");
            model.addConstr(p[a][b] <= z[get_directed_edge_G(j,h,node_pair_to_zidx_G)][node_pair_to_zidx_H[l][m]] + z[get_directed_edge_G(j,h,node_pair_to_zidx_G)][node_pair_to_zidx_H[m][l]], "Path UB2");   
            
        }
    }
}

void GeomGMDForiSolver::add_cyclic_order_constraints()
{
    
    for (size_t a = 0; a < directed_paths_G.size(); a++) {
        auto [i, j, h, alphaijh] = directed_paths_G[a];
        for (size_t b = 0; b < directed_paths_H.size(); b++) {
            auto [k, l, m, alphaklm] = directed_paths_H[b];

            // Constraints forcing cyclic ordering
            auto c_1 = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
            auto c_2 = model.addVar(0.0, 1.0, 0.0, GRB_BINARY);
            model.addConstr(c_1 + c_2 >= 1);
            model.addConstr(p[a][b] <= 1 - c_1);
            GRBLinExpr lhs = 0;
            for(auto const g : G.get_neighbor_sequence(j, i, h)){
                for(auto const n : H.get_neighbor_sequence(l, m, k)){
                    lhs += z[get_directed_edge_G(j,g,node_pair_to_zidx_G)][node_pair_to_zidx_H[l][n]] 
                         + z[get_directed_edge_G(j,g,node_pair_to_zidx_G)][node_pair_to_zidx_H[n][l]];
                }
            }
            model.addConstr(lhs <= std::max(G.get_cyclic_order_distance(j, i, h), H.get_cyclic_order_distance(l, m, k)) * (1 - c_2));
        }
    }
}
