#ifndef GEOMGMD_UTILITY_EDGEMAP_H
#define GEOMGMD_UTILITY_EDGEMAP_H

#include "utility/graph_base.h"

#include <algorithm>
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

// The same edge map with the roles of G and H exchanged (g and h swapped in every pair and in the graph ids).
// Pairs are ordered as the solver returns them: mapped and deleted edges by their edge in the new G,
// then inserted edges by their edge in the new H.
inline EdgeMap transposed(EdgeMap const &edgemap)
{
    EdgeMap result{edgemap.h_graph_id, edgemap.g_graph_id, {}};
    result.edge_pairs.reserve(edgemap.edge_pairs.size());
    for (auto const &pair : edgemap.edge_pairs) {
        result.edge_pairs.emplace_back(pair.h, pair.g);
    }
    std::sort(result.edge_pairs.begin(), result.edge_pairs.end(),
              [](EdgeMap::EdgePair const &a, EdgeMap::EdgePair const &b) {
                  if (a.g.has_value() != b.g.has_value()) return a.g.has_value();  // insertions last
                  return a.g.has_value() ? *a.g < *b.g : *a.h < *b.h;
              });
    return result;
}

#endif // GEOMGMD_UTILITY_EDGEMAP_H
