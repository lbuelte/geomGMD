#include "io/edgemap_io.h"

#include "json/json.hpp"
#include "utility/geometric_graph.h"

#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <cmath>

using json = nlohmann::json;

json edgemap_to_json(EdgeMap const &edgemap, bool optimal)
{
    json j;
    j["G_id"] = edgemap.g_graph_id;
    j["H_id"] = edgemap.h_graph_id;
    j["optimal"] = optimal;

    j["edge_map"] = json::array();
    for (const auto& e : edgemap.edge_pairs) {
        json entry;
        entry["g"] = e.g.has_value() ? json(*e.g) : json(nullptr);
        entry["h"] = e.h.has_value() ? json(*e.h) : json(nullptr);
        j["edge_map"].push_back(entry);
    }
    return j;
}

EdgeMap edgemap_from_json(json const &j)
{
    auto read_opt = [](const json &entry, const char *key) -> std::optional<EdgeID> {
        const json &v = entry.at(key);
        if (v.is_null()) return std::nullopt;
        return v.get<EdgeID>();
    };

    EdgeMap edgemap;
    edgemap.g_graph_id = j.at("G_id").get<int>();
    edgemap.h_graph_id = j.at("H_id").get<int>();
    const json &arr = j.at("edge_map");
    edgemap.edge_pairs.reserve(arr.size());
    for (const auto &entry : arr) {
        // EdgePair's constructor throws if both are nullopt
        edgemap.edge_pairs.emplace_back(read_opt(entry, "g"), read_opt(entry, "h"));
    }
    return edgemap;
}

json edgemap_json(GeomGMDResult const &result)
{
    return edgemap_to_json(result.edge_map, result.statistics.is_optimal());
}
