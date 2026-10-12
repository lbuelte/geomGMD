// Created by Laura Bülte on 2026-04-01.

#ifndef POLYGONCLUSTERING_NODEMAP_IO_H
#define POLYGONCLUSTERING_NODEMAP_IO_H

#include "json/json.hpp"
#include "geometric_graph.h"

#include <optional>

using json = nlohmann::json;

class NodeMap {

public:
        struct NodePair {
        std::optional<VertexID> g; // node in G or nullopt (insertion)
        std::optional<VertexID> h; // node in H or nullopt (deletion)

        NodePair(std::optional<VertexID> g_, std::optional<VertexID> h_)
            : g(g_), h(h_) 
        {
            if (!g && !h) {
                throw std::invalid_argument("Pair cannot be (dummy, dummy)");
            }
        }

        bool is_substitution() const { return g && h; }
        bool is_insertion() const { return !g && h; }
        bool is_deletion() const { return g && !h; }
    };

NodeMap(bool const optimal_, std::vector<NodePair> pairs, GeometricGraph const &G_, GeometricGraph const &H_)
: optimal(optimal_)
, node_pairs(std::move(pairs))
, G(G_)
, H(H_)
{}
    const bool optimal;

    json to_json(GraphID g, GraphID h){
        json j;
        j["G_id"] = g;
        j["H_id"] = h;
        j["optimal"] = optimal;

        j["node_map"] = json::array();

        for (const auto& p : node_pairs) {
            json entry;

            entry["g"] = p.g.has_value() ? json(*p.g) : json(nullptr);
            entry["h"] = p.h.has_value() ? json(*p.h) : json(nullptr);

            j["node_map"].push_back(entry);
        }
        return j;
    }

private:
    std::vector<NodePair> node_pairs;

    const GeometricGraph &G;
    const GeometricGraph &H;
};

#endif // POLYGONCLUSTERING_NODEMAP_IO_H