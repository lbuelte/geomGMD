#ifndef GEOMGMD_UTILITY_GEOMETRIC_GRAPH_H
#define GEOMGMD_UTILITY_GEOMETRIC_GRAPH_H

#include <cstddef>   // size_t
#include <optional>  // std::optional
#include <utility>   // std::pair
#include <vector>    // std::vector
#include <span>      // std::span
#include <string>    // std::string
#include <cassert>   // assert
#include <stdexcept> // std::logic_error

#include "utility/geometry_base.h"
#include "utility/graph_base.h"


// This is a graph data structure where the adjacencies always 
// respect the cyclic order of the edges that are given 
// by the geometric embedding into the Euclidean plane.
class GeometricGraph {

public:

    struct Vertex {
        const VertexID id;
        const Point p;
    };

    struct Edge {
        const VertexID v1;
        const VertexID v2;
        const double length;
    };

    // Reading a graph from JSON: see io/graph_io.h.
    GeometricGraph(std::vector<Vertex> vertices_, 
                   std::vector<Edge> edges_, 
                   std::optional<PolygonID> polygon_id_ = std::nullopt, 
                   std::optional<GraphID> graph_id_ = std::nullopt)
        : _polygon_id(polygon_id_)
        , _graph_id(graph_id_)
        , _vertices(vertices_)
        , _edges(edges_)
    {
        build_ordered_adjacency_lists();
    }

    bool operator<(const GeometricGraph &other) const {
        return this->num_edges() < other.num_edges();
    }
    bool operator<=(const GeometricGraph &other) const {
        return this->num_edges() <= other.num_edges();
    }

    void set_graph_id(GraphID id_){
        _graph_id = id_;
    }

    // Graph id: position in the input file (set for all graphs read from a file). graph_id() throws
    // std::logic_error if it is not set; optional_graph_id() is for code that also handles graphs without id.
    bool has_graph_id() const { return _graph_id.has_value(); }
    GraphID graph_id() const {
        if (not _graph_id.has_value()) {
            throw std::logic_error("GeometricGraph::graph_id: the graph has no graph id");
        }
        return _graph_id.value();
    }
    std::optional<GraphID> optional_graph_id() const { return _graph_id; }
    // Polygon id: the optional "id" of the graph in the input file
    std::optional<PolygonID> polygon_id() const { return _polygon_id; }

    EdgeID num_edges() const { return _edges.size(); }
    VertexID num_vertices() const {return _vertices.size(); }

    Vertex vertex(VertexID v) const { 
        assert(v < _vertices.size());
        return _vertices[v]; 
    }
    Edge edge(EdgeID e) const { 
        assert(e < _edges.size());
        return _edges[e]; 
    }

    auto vertices() const {
        return std::span<const Vertex>(_vertices);
    }
    auto edges() const {
        return std::span<const Edge>(_edges);
    }
    auto neighbors(VertexID v) const {
        assert(v < _ordered_adjacency_list.size());
        return std::span<const VertexID>(_ordered_adjacency_list[v]);
    }
    NeighborID degree(VertexID v) const { 
        assert(v < _ordered_adjacency_list.size());
        return _ordered_adjacency_list[v].size(); 
    }

    // Returns the neighbors of center that come in the order after start and before end (excluding both)
    std::vector<VertexID> get_neighbor_sequence(VertexID center, VertexID start, VertexID end) const;
    // Returns number of neighbors of center that come in cyclic order between start and end (excluding both)
    VertexID get_cyclic_order_distance(VertexID center, VertexID start, VertexID end) const;

private :

    bool check_cyclic_order() const;
    bool check_match_of_adjacencies() const;

    void build_ordered_adjacency_lists();

    const std::optional<PolygonID> _polygon_id;

    std::optional<GraphID> _graph_id;

    std::vector<Vertex> _vertices;
    std::vector<Edge> _edges;
    // This adjacency list always respects the cyclic order of the neighbors
    std::vector<std::vector<VertexID>> _ordered_adjacency_list; 
    std::vector<std::vector<EdgeID>> _adjacency_list_edge_association;
};

// The graph id as text for messages, "?" if the graph has none
inline std::string graph_id_text(GeometricGraph const &G)
{
    return G.has_graph_id() ? std::to_string(G.graph_id()) : "?";
}

#endif // GEOMGMD_UTILITY_GEOMETRIC_GRAPH_H