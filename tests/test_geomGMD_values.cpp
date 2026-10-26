// geomGMD of all pairs of the tiny example graphs with the paper settings must equal the values computed with
// the sigspatial26 version (the code of the paper results). Also checks that every returned edge map is a
// valid constrained edge map whose cost equals the distance, and that geomGMD is symmetric.
// Needs a Gurobi licence (skipped otherwise).
//
// Usage: test_geomGMD_values <examples/tiny/tiny_graphs.json>

#include "check.h"
#include "distance/geomGMD.h"
#include "distance/mapping_cost.h"
#include "io/graph_io.h"

#include <iostream>
#include <set>
#include <variant>
#include <vector>

namespace {

// geomGMD (lb = ub, optimal) of the pairs (g, h), computed with the sigspatial26 version
// (.claude regression harness, paper settings, Gurobi 12.0.3), printed with full precision.
struct Reference {
    GraphID g, h;
    double value;
};
std::vector<Reference> const reference = {
    {0, 1, 4.2632564145606011e-14},  // rectangle and its rotated, translated copy: 0 up to rounding
    {0, 2, 18.798446903082176},
    {0, 3, 21.769009651559315},
    {1, 2, 18.798446903082191},
    {1, 3, 21.769009651559315},
    {2, 3, 25.626874027828386},
};

bool adjacent(GeometricGraph const &G, EdgeID e, EdgeID f)
{
    auto const a = G.edge(e);
    auto const b = G.edge(f);
    return a.v1 == b.v1 or a.v1 == b.v2 or a.v2 == b.v1 or a.v2 == b.v2;
}

// Every edge appears exactly once, and two distinct mapped edges are adjacent in G iff their images are
// adjacent in H (paper, Definition 3.4).
bool is_valid_constrained_edge_map(GeometricGraph const &G, GeometricGraph const &H, EdgeMap const &map)
{
    std::multiset<EdgeID> seen_g, seen_h;
    std::vector<std::pair<EdgeID, EdgeID>> mapped;
    for (auto const &pair : map.edge_pairs) {
        if (pair.g) seen_g.insert(*pair.g);
        if (pair.h) seen_h.insert(*pair.h);
        if (pair.g and pair.h) mapped.emplace_back(*pair.g, *pair.h);
    }
    auto const each_once = [](std::multiset<EdgeID> const &seen, EdgeID num_edges) {
        if (seen.size() != num_edges) return false;
        EdgeID expected = 0;
        for (EdgeID e : seen) if (e != expected++) return false;
        return true;
    };
    if (not each_once(seen_g, G.num_edges()) or not each_once(seen_h, H.num_edges())) return false;
    for (auto const &[g1, h1] : mapped) {
        for (auto const &[g2, h2] : mapped) {
            if (g1 != g2 and adjacent(G, g1, g2) != adjacent(H, h1, h2)) return false;
        }
    }
    return true;
}

bool gurobi_available()
{
    try {
        GurobiEnvManager::getEnv();
        return true;
    } catch (GRBException const &e) {
        std::cerr << "Gurobi is not available (" << e.getMessage() << "), test skipped\n";
        return false;
    }
}

} // end of anonymous namespace

int main(int argc, char* argv[])
{
    if (argc != 2) return 2;
    if (not gurobi_available()) return geomgmd_test::skip_code;

    std::vector<GeometricGraph> graphs;
    read_graph_vector_from_json(graphs, argv[1]);
    ModelParameters const model;     // paper settings
    SolverSettings const settings;   // paper settings

    for (auto const &[g, h, value] : reference) {
        for (bool const swap : {false, true}) {
            GeometricGraph const &G = graphs.at(swap ? h : g);
            GeometricGraph const &H = graphs.at(swap ? g : h);
            auto const r = compute_geomGMD(G, H, model, settings);
            if (not r.distance) {
                geomgmd_test::fail(__FILE__, __LINE__, "distance computed");
                std::cerr << "    pair " << G.graph_id() << ", " << H.graph_id() << ": "
                          << distance_result_error_message(r.result) << "\n";
                continue;
            }
            CHECK(std::holds_alternative<ExactDistance>(r.result));
            CHECK_NEAR(r.distance->lb, value, 1e-9);
            CHECK_NEAR(r.distance->ub, value, 1e-9);
            CHECK(r.edge_map.g_graph_id == G.graph_id());
            CHECK(r.edge_map.h_graph_id == H.graph_id());
            CHECK(is_valid_constrained_edge_map(G, H, r.edge_map));
            CHECK_NEAR(compute_mapping_cost(G, H, r.edge_map, model).total(), r.distance->ub, 1e-9);
        }
    }

    return geomgmd_test::check_result();
}
