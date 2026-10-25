#include "utility/geometric_graph.h"

#include <algorithm>
#include <string>
#include <stdexcept>

namespace {

    // Helper to check whether a point is above or below the x-axis
    bool upperHalf(double x, double y) {
        return (y > 0) || (y == 0 && x > 0);
    }

    // Helper to compute the cross product between points a and b
    double cross(double ax, double ay, double bx, double by) {
        return ax * by - ay * bx;
    }

    // Position of node id among the neighbors of center; callers only pass neighbors, so anything else is a
    // programming error
    std::optional<NeighborID> neighbor_index(std::vector<NodeID> const &neighbors, NodeID center, NodeID id,
                                             char const *function)
    {
        auto it = std::find(neighbors.begin(), neighbors.end(), id);
        if (it == neighbors.end()) {
            throw std::logic_error(std::string("GeometricGraph::") + function + ": node " + std::to_string(id)
                                   + " is not a neighbor of node " + std::to_string(center));
        }
        return std::distance(neighbors.begin(), it);
    }

} // end of anonymous namespace


std::vector<NodeID> GeometricGraph::get_neighbor_sequence(NodeID center, NodeID start, NodeID end) const
{
    const auto neighbors = _ordered_adjacency_list[center];
   

    auto const start_idx = neighbor_index(neighbors, center, start, "get_neighbor_sequence");
    auto const end_idx = neighbor_index(neighbors, center, end, "get_neighbor_sequence");
    if (neighbors.size() <= 2) return std::vector<NodeID>();

    std::vector<NodeID> neighbor_sequence;
    for (NeighborID w = start_idx.value() + 1; w < neighbors.size(); ++w){
        if (w >= end_idx.value()) break;
        neighbor_sequence.push_back(neighbors[w]);
    }
    if (end_idx.value() < start_idx.value()){
        for (NeighborID w = 0; w < end_idx.value(); ++w){
            neighbor_sequence.push_back(neighbors[w]);
        }
    }
    return neighbor_sequence;
}


NodeID GeometricGraph::get_cyclic_order_distance(NodeID center, NodeID start, NodeID end) const
{
    const auto neighbors = _ordered_adjacency_list[center];
    auto const start_idx = neighbor_index(neighbors, center, start, "get_cyclic_order_distance");
    auto const end_idx = neighbor_index(neighbors, center, end, "get_cyclic_order_distance");
    if (start_idx.value() < end_idx.value()){
        return end_idx.value() - start_idx.value() - 1;
    }
    else if (end_idx.value() < start_idx.value()){
        return neighbors.size() - start_idx.value() - 1 + end_idx.value();
    }
    return 0;
}


bool GeometricGraph::check_cyclic_order() const
{
    for (NodeID v = 0; v < num_nodes(); ++v){
        const auto neighbors = _ordered_adjacency_list[v];
        const NodeID n = neighbors.size();
        if (n < 2) continue;

        const Point &p = _nodes[v].p;
        auto isLessOrEqual = [&](Point const &p1, Point const &p2) {
            double dx1 = p1.x - p.x;
            double dy1 = p1.y - p.y;
            double dx2 = p2.x - p.x;
            double dy2 = p2.y - p.y;

            bool up1 = upperHalf(dx1, dy1);
            bool up2 = upperHalf(dx2, dy2);

            if (up1 != up2)
                return up1;  // upper half first

            return cross(dx1, dy1, dx2, dy2) >= 0;   // allow collinear
        };

        // Find index of neighbor with minimal angle
        NodeID min_idx = 0;
        for (NodeID i = 1; i < n; ++i) {
            const Point& pi = _nodes[neighbors[i]].p;
            const Point& pmin = _nodes[neighbors[min_idx]].p;
            if (!isLessOrEqual(pmin, pi))  // pmin > pi
                min_idx = i;
        }

        // Check linear order starting from minimal
        for (NodeID w = 0; w < n - 1; ++w) {
            const NodeID i = (min_idx + w) % n;
            const NodeID j = (min_idx + w + 1) % n;

            const Point& curr = _nodes[neighbors[i]].p;
            const Point& next = _nodes[neighbors[j]].p;

            if (!isLessOrEqual(curr, next))
                return false;
        }
    }
    return true;
}


bool GeometricGraph::check_match_of_adjacencies() const
{
    for (NodeID v = 0; v < num_nodes(); ++v){
        for (NodeID w = 0; w < _ordered_adjacency_list[v].size(); ++w){
            Edge e = _edges[_adjacency_list_edge_association[v][w]];

            auto matches = [&e](NodeID a, NodeID b) {
                return (e.v1 == a and e.v2 == b)
                    or (e.v1 == b and e.v2 == a);
            };
            if (not matches(v, _ordered_adjacency_list[v][w]))
                return false;            
        }
    }
    return true;
}


void GeometricGraph::build_ordered_adjacency_lists()
{
    // Store edge ids in end node entries
    _adjacency_list_edge_association = std::vector<std::vector<EdgeID>>(num_nodes());
    for (EdgeID e = 0; e < num_edges(); ++e){
        auto i = _edges[e].v1;
        auto j = _edges[e].v2;
        _adjacency_list_edge_association[i].push_back(e);
        _adjacency_list_edge_association[j].push_back(e);
    }

    for (NodeID v = 0; v < num_nodes(); ++v){
        // Sort edges anti-clockwise starting from the positive x-axis
        std::sort(_adjacency_list_edge_association[v].begin(), 
                  _adjacency_list_edge_association[v].end(), 
                  [&](EdgeID const &f1, EdgeID const &f2){

            const Point &p = _nodes[v].p;
            const Edge &e1 = _edges[f1];
            const Edge &e2 = _edges[f2];

            const Point &n1 = _nodes[(e1.v1 == v) ? e1.v2 : e1.v1].p;
            const Point &n2 = _nodes[(e2.v1 == v) ? e2.v2 : e2.v1].p;

            double dx1 = n1.x - p.x;
            double dy1 = n1.y - p.y;
            double dx2 = n2.x - p.x;
            double dy2 = n2.y - p.y;

            bool up1 = upperHalf(dx1, dy1);
            bool up2 = upperHalf(dx2, dy2);
            if (up1 != up2)
                return up1;  // upper half comes first

            return cross(dx1, dy1, dx2, dy2) > 0;
        });
    }

    _ordered_adjacency_list = std::vector<std::vector<NodeID>>(num_nodes());
    for (NodeID v = 0; v < num_nodes(); ++v){
        for (auto const &e: _adjacency_list_edge_association[v]){
            NodeID other = (_edges[e].v1 == v)
                    ? _edges[e].v2 
                    : _edges[e].v1;
            _ordered_adjacency_list[v].push_back(other);
        }
    }

    if (not check_cyclic_order()){
        throw std::runtime_error("Error: In build adjacency list: The cyclic order is violated!");
    }
    if (not check_match_of_adjacencies()){
        throw std::runtime_error("Error: In build adjacency list: The edge and node adjacencies do not match!");
    }
}
