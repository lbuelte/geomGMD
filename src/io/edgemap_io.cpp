// Created by Laura Bülte on 2026-04-09

#include "io/edgemap_io.h"

#include "json/json.hpp"
#include "utility/geometric_graph.h"

#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <cmath>

std::optional<json> find_graph_mapping_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id,
        bool &g_before_h) 
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            json entry = json::parse(line);

            if (entry.contains("G_id") && entry.contains("H_id")) {
                GraphID G = entry["G_id"];
                GraphID H = entry["H_id"];

                if ((G == g_id && H == h_id)){
                    g_before_h = true;                    
                    return entry;
                }
                if ((G == h_id && H == g_id)) {
                    g_before_h = false;                    
                    return entry;
                }
            }
        } catch (const json::parse_error&) {
            continue;
        }
    }
    return std::nullopt;
}


std::optional<double> find_geomGMD_value_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }
    std::string line;

    std::getline(file, line); // skip first line
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;

        std::string i_str, j_str, lb_str, ub_str, optimal_str;

        // Read columns (assuming comma-separated CSV)
        std::getline(ss, i_str, ',');
        std::getline(ss, j_str, ',');
        std::getline(ss, lb_str, ',');
        std::getline(ss, ub_str, ',');
        std::getline(ss, optimal_str, ',');

        // Convert i and j
        GraphID i = static_cast<GraphID>(std::stoll(i_str));
        GraphID j = static_cast<GraphID>(std::stoll(j_str));

        // Check unordered match
        if ((i == g_id && j == h_id) || (i == h_id && j == g_id)) {
            double ub = std::stod(ub_str);
            return ub;
        }
    }

    // Not found
    return std::nullopt;
}

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
