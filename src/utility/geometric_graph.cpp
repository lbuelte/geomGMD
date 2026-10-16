#include "utility/geometric_graph.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

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

    std::optional<PolygonID> read_shape_id_from_file(const std::string& json_filename) {
        std::ifstream file(json_filename);
        if (!file) {
            throw std::runtime_error("Could not open file: " + json_filename);
        }

        nlohmann::json j;
        file >> j;

        if (j.contains("id")) {
            return j["id"].get<size_t>();
        }
        return std::nullopt;
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

GeometricGraph::GeometricGraph(const std::string& json_filename, 
                               std::optional<GraphID> graph_id_)
    : _polygon_id(read_shape_id_from_file(json_filename))
    , _graph_id(graph_id_)
{
   std::ifstream file(json_filename);
    if (!file) {
        throw std::runtime_error("Could not open file: " + json_filename);
    }

    nlohmann::json j;
    file >> j;

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


void GeometricGraph::write_svg(
        const std::string &filename, double padding, double canvas_size) const 
{
    double radius = 2.0; // fixed small radius for visualization

    if (_vertices.empty()) {
        return;
    }

    // ---- bounding box ----
    double min_x = _vertices[0].p.x;
    double min_y = _vertices[0].p.y;
    double max_x = min_x;
    double max_y = min_y;

    for (const auto& v : _vertices) {
        min_x = std::min(min_x, v.p.x);
        min_y = std::min(min_y, v.p.y);
        max_x = std::max(max_x, v.p.x);
        max_y = std::max(max_y, v.p.y);
    }

    double width  = max_x - min_x;
    double height = max_y - min_y;

    if (width == 0 || height == 0) return;

    // ---- compute scaling factor ----
    double scale_x = (canvas_size - 2*padding) / width;
    double scale_y = (canvas_size - 2*padding) / height;
    double scale = std::min(scale_x, scale_y);

    // ---- helper lambda to scale coordinates ----
    auto sx = [&](double x) { return (x - min_x) * scale + padding; };
    auto sy = [&](double y) { return canvas_size - ((y - min_y) * scale + padding); }; // flip Y

    std::ofstream svg(filename);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << canvas_size
        << "' height='" << canvas_size
        << "' viewBox='0 0 " << canvas_size << " " << canvas_size << "'>\n";

    svg << "<rect x='0' y='0' width='" << canvas_size
        << "' height='" << canvas_size
        << "' style='fill:white'/>\n";

    // ---- draw medial axis edges ----
    for (size_t k = 0; k < _edges.size(); ++k) {
        int i1 = _edges[k].v1;
        int i2 = _edges[k].v2;
        const auto& p1 = _vertices[i1].p;
        const auto& p2 = _vertices[i2].p;

        svg << "<line x1='" << sx(p1.x) << "' y1='" << sy(p1.y)
            << "' x2='" << sx(p2.x) << "' y2='" << sy(p2.y)
            << "' style='stroke:green;stroke-width:2'/>\n";
        svg << "'/>\n";

    }

    // ---- draw medial axis vertices ----
    for (const auto& v : _vertices) {
        svg << "<circle cx='" << sx(v.p.x) << "' cy='" << sy(v.p.y)
            << "' r='" << radius << "' fill='black' stroke='black' stroke-width='2'/>\n";
    }

    // ---- legend with graph size ----
    std::size_t num_vertices = _vertices.size();
    std::size_t num_edges    = _edges.size();

    double legend_x = 15;
    double legend_y = 25;

    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Vertices: " << num_vertices
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 18)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Edges: " << num_edges
        << "</text>\n";

        svg << "</svg>\n";
        svg.close();
}


void GeometricGraph::prune_edges(const std::vector<bool>& remove_mask)
{
    // Compute vertex degrees without removed edges    
    auto num_edges_to_remove = 0;
    std::vector<int> degree(num_vertices(), 0);
    for (EdgeID e = 0; e < num_edges(); ++e){
        if (remove_mask[e]){
            num_edges_to_remove++;
            continue;
        }
        const Edge& ed = _edges[e];
        degree[ed.v1]++;
        degree[ed.v2]++;
    }
    if (num_edges_to_remove == 0){
        return;
    }
    bool has_singleton = false;
    for (VertexID v = 0; v < num_vertices(); ++v){
        if (degree[v] == 0){
            has_singleton = true;
            break;
        }
    }

    // If there are no singletons, rebuild edges and return
    if (not has_singleton) {
    
        std::vector<Edge> new_edges;
        new_edges.reserve(num_edges() - num_edges_to_remove);
        for (EdgeID e = 0; e < num_edges(); ++e){
            if (remove_mask[e]){
                continue;
            }
            const Edge& ed = edge(e);
            new_edges.emplace_back(ed.v1,
                                ed.v2,
                                ed.length);
        }
        _edges.swap(new_edges);
        build_ordered_adjacency_lists();
        return;
    }

    // Build new vertices without singletons
    std::vector<Vertex> new_vertices;
    std::vector<VertexID> remap(num_vertices(), std::numeric_limits<VertexID>::max());
    for (VertexID v = 0; v < num_vertices(); ++v){
        if (degree[v] > 0){
            remap[v] = new_vertices.size();
            new_vertices.push_back(_vertices[v]);
        }
    }

    // Rebuild edges with new vertex ids
    std::vector<Edge> new_edges;
    new_edges.reserve(num_edges() - num_edges_to_remove);
    for (EdgeID e = 0; e < num_edges(); ++e){
        if (remove_mask[e]){
            continue;
        }
        const Edge& ed = edge(e);
        new_edges.emplace_back(remap[ed.v1],
                               remap[ed.v2],
                               ed.length);
    }

    // Swap out vertices, edges and rebuild adjacency lists
    _vertices.swap(new_vertices);
    _edges.swap(new_edges);
    build_ordered_adjacency_lists();
}


void GeometricGraph::prune_edges(const std::vector<EdgeID>& edges_to_remove)
{
    if (edges_to_remove.empty()){
        return;
    }

    std::vector<bool> remove_mask(num_edges(), false);
    for (EdgeID e : edges_to_remove){
        remove_mask[e] = true;    
    }
    prune_edges(remove_mask);
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


void read_graph_vector_from_jsons(std::vector<GeometricGraph>& graphs,
                                  const std::string& filepath)
{
    graphs.clear();

    std::filesystem::path folder(filepath);

    if (!std::filesystem::exists(folder) ||
        !std::filesystem::is_directory(folder))
    {
        throw std::runtime_error("Invalid directory: " + filepath);
    }

    // Match: graph_<number>.json
    std::regex pattern(R"(graph_(\d+)\.json)");

    for (const auto& entry : std::filesystem::directory_iterator(folder))
    {
        if (!entry.is_regular_file())
            continue;

        const std::filesystem::path& path = entry.path();

        if (path.extension() != ".json")
            continue;

        std::string filename = path.filename().string();
        std::smatch match;

        if (!std::regex_match(filename, match, pattern))
            continue;

        // Extract ID from filename
        GraphID id = static_cast<GraphID>(std::stoull(match[1].str()));

        // Construct graph with filename + extracted ID
        graphs.emplace_back(path.string(), std::make_optional(id));
    }
}

double get_mean_edge_length(std::vector<GeometricGraph> const &graphs)
{
    double total_edge_lengths = 0;
    double total_num_edges = 0;
    for (auto const &graph : graphs){
        for (auto const &edge: graph.edges()){
            total_edge_lengths += edge.length;
            ++total_num_edges;
        }
    }
    return total_edge_lengths / total_num_edges;
}

double get_mean_edge_length(std::vector<const GeometricGraph *> const &graphs)
{
    double total_edge_lengths = 0;
    double total_num_edges = 0;
    for (auto const &graph : graphs){
        for (auto const &edge: graph->edges()){
            total_edge_lengths += edge.length;
            ++total_num_edges;
        }
    }
    return total_edge_lengths / total_num_edges;
}

double get_mean_edge_length(std::vector<GeometricGraph>::iterator begin, std::vector<GeometricGraph>::iterator end)
{
    double total_edge_lengths = 0;
    double total_num_edges = 0;
    for (auto graph_it = begin; graph_it < end; ++graph_it){
        for (auto const &edge: graph_it->edges()){
            total_edge_lengths += edge.length;
            ++total_num_edges;
        }
    }
    return total_edge_lengths / total_num_edges;
}

double get_mean_edge_length(std::span<GeometricGraph> const &graphs)
{
    double total_edge_lengths = 0;
    double total_num_edges = 0;
    for (auto const &graph : graphs){
        for (auto const &edge: graph.edges()){
            total_edge_lengths += edge.length;
            ++total_num_edges;
        }
    }
    return total_edge_lengths / total_num_edges;
}

