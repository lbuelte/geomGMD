#include "io/graph_io.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

    double euclidean_length(Point const &a, Point const &b){
        return std::hypot(b.x - a.x, b.y - a.y);
    }

    std::optional<PolygonID> read_shape_id_from_json(const nlohmann::json& j) {
        if (j.contains("id")) {
            return j["id"].get<size_t>();
        }
        return std::nullopt;
    }

} // end of anonymous namespace


GeometricGraph graph_from_json(const nlohmann::json& j, std::optional<GraphID> graph_id)
{
    if (!j.contains("medial_axis") ||
        !j["medial_axis"].contains("vertices") ||
        !j["medial_axis"].contains("edges")) {
        throw std::runtime_error("Invalid JSON structure.");
    }

    std::vector<GeometricGraph::Vertex> vertices;
    const auto& vertices_json = j["medial_axis"]["vertices"];
    for (VertexID i = 0; i < vertices_json.size(); ++i) {

        double x = vertices_json[i]["x"].get<double>();
        double y = vertices_json[i]["y"].get<double>();
        vertices.emplace_back(i, Point(x, y));
    }

    std::vector<GeometricGraph::Edge> edges;
    for (const auto& e : j["medial_axis"]["edges"]) {

        VertexID source = e["source"].get<size_t>();
        VertexID target = e["target"].get<size_t>();
        const auto length = euclidean_length(vertices[source].p, vertices[target].p);
        edges.emplace_back(source, target, length);
    }
    return GeometricGraph(std::move(vertices), std::move(edges), read_shape_id_from_json(j), graph_id);
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
        graphs.push_back(graph_from_json(graph_json, gid));
        ++gid;
    }
}
