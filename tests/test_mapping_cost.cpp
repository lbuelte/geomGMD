// Cost of given constrained edge maps (compute_mapping_cost, paper Eq. (2)-(4)) on the tiny example graphs.
// No Gurobi computation.
//
// Usage: test_mapping_cost <examples/tiny/tiny_graphs.json>

#include "check.h"
#include "distance/mapping_cost.h"
#include "io/graph_io.h"

#include <cmath>
#include <numbers>
#include <vector>

namespace {

double total_length(GeometricGraph const &G)
{
    double sum = 0;
    for (auto const &e : G.edges()) sum += e.length;
    return sum;
}

// Edge map with the given mapped pairs; all other edges are deleted (G) or inserted (H)
EdgeMap edge_map(GeometricGraph const &G, GeometricGraph const &H, std::vector<std::pair<EdgeID, EdgeID>> mapped)
{
    EdgeMap map;
    std::vector<bool> mapped_g(G.num_edges(), false), mapped_h(H.num_edges(), false);
    for (auto const &[g, h] : mapped) {
        map.edge_pairs.emplace_back(g, h);
        mapped_g[g] = mapped_h[h] = true;
    }
    for (EdgeID g = 0; g < G.num_edges(); ++g) if (not mapped_g[g]) map.edge_pairs.emplace_back(g, std::nullopt);
    for (EdgeID h = 0; h < H.num_edges(); ++h) if (not mapped_h[h]) map.edge_pairs.emplace_back(std::nullopt, h);
    return map;
}

} // end of anonymous namespace

int main(int argc, char* argv[])
{
    if (argc != 2) return 2;
    std::vector<GeometricGraph> graphs;
    read_graph_vector_from_json(graphs, argv[1]);
    GeometricGraph const &rectangle = graphs.at(0);
    GeometricGraph const &rotated = graphs.at(1);
    GeometricGraph const &square = graphs.at(3);
    ModelParameters const paper;  // c_id = c_m = c_r = 1, signed angles

    double const pi = std::numbers::pi;
    double const s18 = std::sqrt(18.0), s32 = std::sqrt(32.0);  // edge lengths to the corners

    // Rectangle -> rotated copy, every edge to itself: cost 0
    {
        auto const c = compute_mapping_cost(rectangle, rotated, edge_map(rectangle, rotated, {{0, 0}, {1, 1}, {2, 2},
                                                                                          {3, 3}, {4, 4}}), paper);
        CHECK_NEAR(c.total(), 0.0, 1e-9);
    }

    // Nothing mapped: all edges of the rectangle deleted, all edges of the square inserted
    {
        auto const map = edge_map(rectangle, square, {});
        auto const c = compute_mapping_cost(rectangle, square, map, paper);
        CHECK_NEAR(c.deletion, total_length(rectangle), 1e-12);
        CHECK_NEAR(c.deletion, 4 + 4 * s18, 1e-12);
        CHECK_NEAR(c.insertion, 4 * s32, 1e-12);
        CHECK_NEAR(c.length_difference, 0.0, 1e-12);
        CHECK_NEAR(c.path_angle, 0.0, 1e-12);
        CHECK_NEAR(c.total(), 4 + 4 * s18 + 4 * s32, 1e-12);

        ModelParameters doubled = paper;
        doubled.cost_factor_deletion_insertion = 2.0;
        CHECK_NEAR(compute_mapping_cost(rectangle, square, map, doubled).total(), 2 * (4 + 4 * s18 + 4 * s32), 1e-12);
    }

    // Rectangle edges 0 (to (7, 3), 0°) and 1 (to (0, 0), 225°) at node 0 mapped to two square edges at its
    // centre (edge 0 to (0, 0), 225°; edge 1 to (8, 0), 315°). Both mapped paths of length two (one per
    // direction) count. The signed angles are measured in (-pi, pi] (as in sigspatial26; the paper defines
    // [0, 2 pi), see the planned angle fix): -135° / 135° in the rectangle. Matching orientation (-90° / 90°
    // in the square): 45° per path. Opposite orientation (90° / -90°): 225° per path ([0, 2 pi): 135°).
    {
        // square edge 1 goes to (8, 0) (315°), edge 0 to (0, 0) (225°)
        auto const map = edge_map(rectangle, square, {{0, 1}, {1, 0}});
        auto const c = compute_mapping_cost(rectangle, square, map, paper);
        CHECK_NEAR(c.deletion, 3 * s18, 1e-12);
        CHECK_NEAR(c.insertion, 2 * s32, 1e-12);
        CHECK_NEAR(c.length_difference, (s32 - 4) + (s32 - s18), 1e-12);
        CHECK_NEAR(c.path_angle, 2 * pi / 4, 1e-12);
        CHECK_NEAR(c.total(), 3 * s18 + 2 * s32 + (s32 - 4) + (s32 - s18) + pi / 2, 1e-12);

        auto const swapped = compute_mapping_cost(rectangle, square, edge_map(rectangle, square, {{0, 0}, {1, 1}}),
                                                  paper);
        CHECK_NEAR(swapped.path_angle, 2 * 5 * pi / 4, 1e-12);

        ModelParameters weighted = paper;
        weighted.cost_factor_length_difference = 3.0;
        weighted.cost_factor_angle_difference = 0.5;
        auto const w = compute_mapping_cost(rectangle, square, map, weighted);
        CHECK_NEAR(w.length_difference, 3 * c.length_difference, 1e-12);
        CHECK_NEAR(w.path_angle, 0.5 * c.path_angle, 1e-12);
        CHECK_NEAR(w.deletion + w.insertion, c.deletion + c.insertion, 1e-12);
    }

    return geomgmd_test::check_result();
}
