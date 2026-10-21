#ifndef GEOMGMD_IO_GRAPH_IO_H
#define GEOMGMD_IO_GRAPH_IO_H

// Reading geometric graphs from JSON. Each graph is an object with an optional polygon "id" and
// "medial_axis": {"vertices": [{"x", "y"}, ...], "edges": [{"source", "target"}, ...]};
// edge lengths are the Euclidean distances of the end nodes.

#include "json/json.hpp"
#include "utility/geometric_graph.h"

#include <optional>
#include <string>
#include <vector>

// Builds a geometric graph from one JSON graph object (see above).
GeometricGraph graph_from_json(const nlohmann::json& j,
                               std::optional<GraphID> graph_id = std::nullopt);

// Reads all graphs of the array "graphs" in the JSON file; graph ids = positions in the array.
void read_graph_vector_from_json(std::vector<GeometricGraph>& graphs,
                                 const std::string& filepath);

#endif // GEOMGMD_IO_GRAPH_IO_H
