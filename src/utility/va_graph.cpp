#include "utility/va_graph.h"

// --- VAGRAPH CLASS ---

GeometricGraph& VAGraph::filter(double vanishing_angle) {
    //immediately returned stored version if possible
    if (filtered_graph.has_value() && vanishing_angle == va_of_stored_filtered_graph) {
        return *filtered_graph;
    }

    // Ensure filtered_graph exists, then copy the source graph
    filtered_graph.emplace(original_graph);        // engages the optional
    GeometricGraph& g = *filtered_graph;       // reference to the actual graph

    // Collect edges below threshold
    std::vector<bool> edge_to_be_removed(g.num_edges(), false);
    for (EdgeID e = 0; e < g.num_edges(); ++e) {
        const GeometricGraph::Edge& edge = g.edge(e);
        if (edge.vanishing_angle < vanishing_angle) {
            edge_to_be_removed[e] = true;
        }
    }
    // Delete edges and resulting singleton vertices
    g.prune_edges(edge_to_be_removed);

    //remember last used vanishing angle
    va_of_stored_filtered_graph = vanishing_angle;

    return g;
}


// --- END VAGRAPH CLASS ---