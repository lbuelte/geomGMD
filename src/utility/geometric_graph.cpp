#include "utility/geometric_graph.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace {

    // Helper to check whether a point is above or below the x-axis
    bool upperHalf(double x, double y) {
        return (y > 0) || (y == 0 && x > 0);
    }

    // Helper to compute the cross product between points a and b
    double cross(double ax, double ay, double bx, double by) {
        return ax * by - ay * bx;
    }

    double euklidean_length(Point const &a, Point const &b){
        return std::hypot(b.x - a.x, b.y - a.y);
    }

     std::optional<PolygonID> read_shape_id_from_json(const nlohmann::json& j) {
        if (j.contains("id")) {
            return j["id"].get<size_t>();
        }
        return std::nullopt;
    }

} // end of anonymous namespace


GeometricGraph::GeometricGraph(const nlohmann::json& j, 
                               std::optional<GraphID> graph_id_)
    : _polygon_id(read_shape_id_from_json(j))
    , _graph_id(graph_id_)
{
    if (!j.contains("medial_axis") ||
        !j["medial_axis"].contains("vertices") ||
        !j["medial_axis"].contains("edges")) {
        throw std::runtime_error("Invalid JSON structure.");
    }

    const auto& vertices = j["medial_axis"]["vertices"];
    for (VertexID i = 0; i < vertices.size(); ++i) {

        double x = vertices[i]["x"].get<double>();
        double y = vertices[i]["y"].get<double>();
        _vertices.emplace_back(i, Point(x, y));
    }

    for (const auto& e : j["medial_axis"]["edges"]) {

        VertexID source = e["source"].get<size_t>();
        VertexID target = e["target"].get<size_t>();
        const auto length = euklidean_length(vertex(source).p, vertex(target).p);
        _edges.emplace_back(source, target, length);
    }
    build_ordered_adjacency_lists();
}


std::vector<VertexID> GeometricGraph::get_neighbor_sequence(VertexID center, VertexID start, VertexID end) const
{
    const auto neighbors = _ordered_adjacency_list[center];
   

    auto idx = [&neighbors](VertexID id) -> std::optional<NeighborID> {
        auto it = std::find(neighbors.begin(), neighbors.end(), id);
        if (it != neighbors.end()) {
            return std::distance(neighbors.begin(), it);
        } else {
            std::cerr << "Warning: GeometricGraph::get_neighbor_sequence was called with vertex" 
                      << id << " not in neighborhood of center!" << std::endl;
            return std::nullopt;
        }
    };
    auto const start_idx = idx(start);
    auto const end_idx = idx(end);
    if (not start_idx.has_value() or not end_idx.has_value()){
        std::cerr << "Warning: GeometricGraph::get_neighbor_sequence: vertex not in neighborhood of center " 
                  << center << "!" << std::endl;
        return std::vector<VertexID>();
    }
    if (neighbors.size() <= 2) return std::vector<VertexID>();

    std::vector<VertexID> neighbor_sequence;
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


VertexID GeometricGraph::get_cyclic_order_distance(VertexID center, VertexID start, VertexID end) const
{
    const auto neighbors = _ordered_adjacency_list[center];
    auto idx = [&neighbors](VertexID id) -> std::optional<NeighborID> {
        auto it = std::find(neighbors.begin(), neighbors.end(), id);
        if (it != neighbors.end()) {
            return std::distance(neighbors.begin(), it);
        } else {
            std::cerr << "Warning: GeometricGraph::get_cyclic_order_distance was called with vertex" 
                      << id << " not in neighborhood of center!" << std::endl;
            return std::nullopt;
        }
    };
    auto const start_idx = idx(start);
    auto const end_idx = idx(end);
    if (not start_idx.has_value() or not end_idx.has_value()){
        std::cerr << "Warning: GeometricGraph::get_cyclic_order_distance: vertex not in neighborhood of center " 
                  << center << "!" << std::endl;
        return 0;
    }
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
    for (VertexID v = 0; v < num_vertices(); ++v){
        const auto neighbors = _ordered_adjacency_list[v];
        const VertexID n = neighbors.size();
        if (n < 2) continue;

        const Point &p = _vertices[v].p;
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
        VertexID min_idx = 0;
        for (VertexID i = 1; i < n; ++i) {
            const Point& pi = _vertices[neighbors[i]].p;
            const Point& pmin = _vertices[neighbors[min_idx]].p;
            if (!isLessOrEqual(pmin, pi))  // pmin > pi
                min_idx = i;
        }

        // Check linear order starting from minimal
        for (VertexID w = 0; w < n - 1; ++w) {
            const VertexID i = (min_idx + w) % n;
            const VertexID j = (min_idx + w + 1) % n;

            const Point& curr = _vertices[neighbors[i]].p;
            const Point& next = _vertices[neighbors[j]].p;

            if (!isLessOrEqual(curr, next))
                return false;
        }
    }
    return true;
}


bool GeometricGraph::check_match_of_adjacencies() const
{
    for (VertexID v = 0; v < num_vertices(); ++v){
        for (VertexID w = 0; w < _ordered_adjacency_list[v].size(); ++w){
            Edge e = _edges[_adjacency_list_edge_association[v][w]];

            auto matches = [&e](VertexID a, VertexID b) {
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
    _adjacency_list_edge_association = std::vector<std::vector<EdgeID>>(num_vertices());
    for (EdgeID e = 0; e < num_edges(); ++e){
        auto i = _edges[e].v1;
        auto j = _edges[e].v2;
        _adjacency_list_edge_association[i].push_back(e);
        _adjacency_list_edge_association[j].push_back(e);
    }

    for (VertexID v = 0; v < num_vertices(); ++v){
        // Sort edges anti-clockwise starting from the positive x-axis
        std::sort(_adjacency_list_edge_association[v].begin(), 
                  _adjacency_list_edge_association[v].end(), 
                  [&](EdgeID const &f1, EdgeID const &f2){

            const Point &p = _vertices[v].p;
            const Edge &e1 = _edges[f1];
            const Edge &e2 = _edges[f2];

            const Point &n1 = _vertices[(e1.v1 == v) ? e1.v2 : e1.v1].p;
            const Point &n2 = _vertices[(e2.v1 == v) ? e2.v2 : e2.v1].p;

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

    _ordered_adjacency_list = std::vector<std::vector<VertexID>>(num_vertices());
    for (VertexID v = 0; v < num_vertices(); ++v){
        for (auto const &e: _adjacency_list_edge_association[v]){
            VertexID other = (_edges[e].v1 == v)
                    ? _edges[e].v2 
                    : _edges[e].v1;
            _ordered_adjacency_list[v].push_back(other);
        }
    }

    if (not check_cyclic_order()){
        throw std::runtime_error("Error: In build adjacency list: The cyclic order is violated!");
    }
    if (not check_match_of_adjacencies()){
        throw std::runtime_error("Error: In build adjacency list: The edge and vertex adjacencies do not match!");
    }
}


void read_graph_vector_from_json(std::vector<GeometricGraph>& graphs,
                                 const std::string& filepath)
{
    graphs.clear();

    std::filesystem::path file_path(filepath);

    if (!std::filesystem::exists(file_path) ||
        !std::filesystem::is_regular_file(file_path))
    {
        throw std::runtime_error("Invalid JSON file: " + filepath);
    }

    // Read JSON file
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file: " + filepath);
    }

    nlohmann::json j;
    file >> j;
    file.close();

    // Validate structure
    if (!j.contains("graphs") || !j["graphs"].is_array())
    {
        throw std::runtime_error("Invalid JSON format: missing 'graphs' array");
    }

    GraphID gid = 0;
    // Read graphs
    for (const auto& graph_json : j["graphs"])
    {
        graphs.emplace_back(graph_json, gid);
        ++gid;
    }
}
