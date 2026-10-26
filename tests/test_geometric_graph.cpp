// Reading geometric graphs from JSON (examples/tiny) and the cyclic order of the neighbours.
//
// Usage: test_geometric_graph <examples/tiny/tiny_graphs.json>

#include "check.h"
#include "io/graph_io.h"
#include "utility/geometric_graph.h"

#include <stdexcept>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc != 2) return 2;
    std::vector<GeometricGraph> graphs;
    read_graph_vector_from_json(graphs, argv[1]);

    CHECK(graphs.size() == 4);
    // Sizes (nodes, edges) of the rectangle, its rotated copy, the L-shape and the square
    std::vector<std::pair<NodeID, EdgeID>> const sizes = {{6, 5}, {6, 5}, {8, 7}, {5, 4}};
    for (GraphID g = 0; g < graphs.size() and g < sizes.size(); ++g) {
        CHECK(graphs[g].num_nodes() == sizes[g].first);
        CHECK(graphs[g].num_edges() == sizes[g].second);
        // graph id = position in the file, polygon id = "id" in the file
        CHECK(graphs[g].graph_id() == g);
        CHECK(graphs[g].polygon_id() == static_cast<PolygonID>(g + 1));
    }

    // Rectangle: node 0 at (3, 3) has the neighbours 1 (7, 3), 2 (0, 0) and 3 (0, 6). Anti-clockwise
    // from the positive x-axis: 1 (0°), 3 (135°), 2 (225°). The rotated copy has the same cyclic order.
    for (GraphID g : {0, 1}) {
        GeometricGraph const &G = graphs[g];
        CHECK(G.degree(0) == 3);
        auto const neighbors = G.neighbors(0);
        CHECK((std::vector<NodeID>(neighbors.begin(), neighbors.end()) == std::vector<NodeID>{1, 3, 2}));
        CHECK(G.get_cyclic_order_distance(0, 1, 2) == 1);  // 3 lies between 1 and 2
        CHECK(G.get_cyclic_order_distance(0, 2, 1) == 0);
        CHECK(G.degree(2) == 1);
    }
    CHECK_NEAR(graphs[0].edge(0).length, 4.0, 1e-12);
    CHECK_NEAR(graphs[1].edge(0).length, 4.0, 1e-12);

    // A graph built in code has no ids
    GeometricGraph const P({{0, {0, 0}}, {1, {1, 0}}}, {{0, 1, 1.0}});
    CHECK(not P.has_graph_id());
    CHECK(not P.optional_graph_id().has_value());
    CHECK(not P.polygon_id().has_value());
    CHECK(graph_id_text(P) == "?");
    CHECK_THROWS(P.graph_id(), std::logic_error);

    return geomgmd_test::check_result();
}
