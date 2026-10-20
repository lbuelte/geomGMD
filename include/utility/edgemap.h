// Created by Laura Bülte on 2026-04-01.

#ifndef GEOMGMD_UTILITY_EDGEMAP_H
#define GEOMGMD_UTILITY_EDGEMAP_H

#include "utility/geometric_graph.h"

#include "json/json.hpp"

#include <optional>

using json = nlohmann::json;


class EdgeMap {

public:
    struct EdgePair {
        std::optional<EdgeID> g; // edge in G or nullopt (insertion)
        std::optional<EdgeID> h; // edge in H or nullopt (deletion)

        EdgePair(std::optional<EdgeID> g_, std::optional<EdgeID> h_)
            : g(g_), h(h_) 
        {
            if (!g && !h) {
                throw std::invalid_argument("Pair cannot be (dummy, dummy)");
            }
        }

        bool operator==(const EdgePair &other) const {
            return this->g == other.g and this->h == other.h;
        }

        bool is_substitution() const { return g && h; }
        bool is_insertion() const { return !g && h; }
        bool is_deletion() const { return g && !h; }
    };

    EdgeMap(bool const optimal_, std::vector<EdgePair> pairs, GeometricGraph const &G_, GeometricGraph const &H_)
        : optimal(optimal_)
        , edge_pairs(std::move(pairs))
    {}

    json to_json(GraphID g, GraphID h) const {
        json j;
        j["G_id"] = g;
        j["H_id"] = h;
        j["optimal"] = optimal;

        j["edge_map"] = json::array();

        for (const auto& e : edge_pairs) {
            json entry;

            entry["g"] = e.g.has_value() ? json(*e.g) : json(nullptr);
            entry["h"] = e.h.has_value() ? json(*e.h) : json(nullptr);

            j["edge_map"].push_back(entry);
        }
        return j;
    }

    static EdgeMap from_json(const json &j,
                             GeometricGraph const &G,
                             GeometricGraph const &H)
    {
        const bool optimal = j.at("optimal").get<bool>();

        auto read_opt = [](const json &entry, const char *key) -> std::optional<EdgeID> {
            const json &v = entry.at(key);
            if (v.is_null()) return std::nullopt;
            return v.get<EdgeID>();
        };

        std::vector<EdgePair> pairs;
        const json &arr = j.at("edge_map");
        pairs.reserve(arr.size());

        for (const auto &entry : arr) {
            // EdgePair's constructor throws if both are nullopt
            pairs.emplace_back(read_opt(entry, "g"), read_opt(entry, "h"));
        }

        return EdgeMap(optimal, std::move(pairs), G, H);
    }

    const bool optimal;
    std::vector<EdgePair> edge_pairs;
};

#endif // GEOMGMD_UTILITY_EDGEMAP_H