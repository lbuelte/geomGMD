// geomGMD minimizes over all constrained edge maps, not only over edge maps induced by a node map (paper,
// Remark 3.5 and 5.6): an edge may be deleted although its end nodes are mapped onto the end nodes of an edge,
// which is then inserted. Needs a Gurobi licence (skipped otherwise).
//
// G and H are "double stars": junctions i and j connected by an edge of length 3, each with two leaves. The
// leaves are the same in G and H (relative to their junction), only the connecting edge is rotated by 90°:
//   G: i = (0, 0), j = (3, 0)       H: k = (0, 0), l = (0, -3)
// Optimum: map the four leaves (cost 0), delete ij and insert kl: 3 + 3 = 6.
// The induced edge map (also mapping ij to kl) costs 4π: the 8 mapped paths through ij each differ by 90°.

#include "check.h"
#include "distance/geomGMD.h"
#include "distance/mapping_cost.h"

#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>

namespace {

// Nodes i, j, leaves of i (-4, 4), (-4, -2), leaves of j at j + (3, 3), j + (3, -3); edge 0 = ij
GeometricGraph double_star(Point j)
{
    std::vector<Point> const p = {{0, 0}, j, {-4, 4}, {-4, -2}, {j.x + 3, j.y + 3}, {j.x + 3, j.y - 3}};
    std::vector<std::pair<NodeID, NodeID>> const e = {{0, 1}, {0, 2}, {0, 3}, {1, 4}, {1, 5}};
    std::vector<GeometricGraph::Node> nodes;
    for (NodeID v = 0; v < p.size(); ++v) nodes.push_back({v, p[v]});
    std::vector<GeometricGraph::Edge> edges;
    for (auto const &[u, v] : e) edges.push_back({u, v, std::hypot(p[v].x - p[u].x, p[v].y - p[u].y)});
    return GeometricGraph(std::move(nodes), std::move(edges));
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

int main()
{
    if (not gurobi_available()) return geomgmd_test::skip_code;

    GeometricGraph const G = double_star({3, 0});
    GeometricGraph const H = double_star({0, -3});
    ModelParameters const model;  // paper settings
    double const pi = std::numbers::pi;

    // The induced edge map (every edge to itself) costs 4π
    EdgeMap induced;
    for (EdgeID e = 0; e < 5; ++e) induced.edge_pairs.emplace_back(e, e);
    CHECK_NEAR(compute_mapping_cost(G, H, induced, model).total(), 4 * pi, 1e-9);

    auto const r = compute_geomGMD(G, H, model, SolverSettings{});
    CHECK(r.distance.has_value());
    if (r.distance) {
        CHECK_NEAR(r.distance->ub, 6.0, 1e-6);
        CHECK_NEAR(r.distance->lb, 6.0, 1e-6);
        // ij (edge 0) deleted, kl (edge 0) inserted, the leaves mapped onto each other
        for (auto const &pair : r.edge_map.edge_pairs) {
            if (pair.g == EdgeID{0}) CHECK(not pair.h.has_value());
            if (pair.h == EdgeID{0}) CHECK(not pair.g.has_value());
            if (pair.g and *pair.g != 0) CHECK(pair.h == pair.g);
        }
        CHECK_NEAR(compute_mapping_cost(G, H, r.edge_map, model).total(), 6.0, 1e-9);
    }

    return geomgmd_test::check_result();
}
