// Created by Laura Bülte on 2026-04-01.

#ifndef GEOMGMD_UTILITY_EDGEMAP_H
#define GEOMGMD_UTILITY_EDGEMAP_H

#include "utility/graph_base.h"

#include <optional>
#include <stdexcept>
#include <vector>

// A constrained edge map between two geometric graphs G and H (see the paper, Definition 3.4),
// given as pairs of edges: mapped edges (g, h), deleted edges of G (g, nullopt) and inserted edges of H
// (nullopt, h). Every edge of G and H appears in exactly one pair.
// The JSON representation is in io/edgemap_io.h.
struct EdgeMap {

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

    int g_graph_id = -1;    // graph id of G (whose edges are the g entries); -1 if not set
    int h_graph_id = -1;    // graph id of H (whose edges are the h entries); -1 if not set
    std::vector<EdgePair> edge_pairs;
};

#endif // GEOMGMD_UTILITY_EDGEMAP_H
