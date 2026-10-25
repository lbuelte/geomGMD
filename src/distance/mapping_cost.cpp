#include "distance/mapping_cost.h"

#include "utility/angle_computation.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

// Path (angle) mapping costs of all mapped paths of length two (body unchanged from
// compute_total_angle_objective in sigspatial26)
double path_angle_cost(
        const ModelParameters &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    std::vector<std::vector<EdgeMap::EdgePair>> G_mapped_edge_adjacencies(G.num_nodes());

    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            G_mapped_edge_adjacencies[G.edge(g).v1].push_back(edge_pair);
            G_mapped_edge_adjacencies[G.edge(g).v2].push_back(edge_pair);
        }
    }

    double objective_value = 0;

    for (NodeID j = 0; j < G.num_nodes(); ++j){
        // If there are at least two mapped edges incident to j, 
        // get path mapping cost for all paths with j in the middle
        if (G_mapped_edge_adjacencies[j].size() < 2) continue;
        for (auto const &edge_pair_1 : G_mapped_edge_adjacencies[j]){
            assert(edge_pair_1.is_substitution());

            // First mapped edge pair in G and H
            EdgeID edge1_G = edge_pair_1.g.value();
            EdgeID edge1_H = edge_pair_1.h.value();

            // Other G-node adjacent to j
            NodeID i = G.edge(edge1_G).v1 == j ? 
                    G.edge(edge1_G).v2 :
                    G.edge(edge1_G).v1;

            for (auto const &edge_pair_2 : G_mapped_edge_adjacencies[j]){
                if (edge_pair_2 == edge_pair_1) continue;
                assert(edge_pair_2.is_substitution());
                
                // Second mapped edge (different from first edge)
                EdgeID edge2_G = edge_pair_2.g.value();
                EdgeID edge2_H = edge_pair_2.h.value();

                // Other G-node adjacent to j and different from i
                NodeID h = (G.edge(edge2_G).v1 == j) ? 
                        G.edge(edge2_G).v2 : 
                        G.edge(edge2_G).v1;

                // Angle for path ijh
                double angle1 = (params.angle_type == AngleType::Signed) ?
                        compute_signed_angle_between_lines(G.node(i).p, G.node(j).p, G.node(h).p) :
                        compute_unsigned_angle_between_lines(G.node(i).p, G.node(j).p, G.node(h).p);

                // Helper to find middle node of corresponding path in H
                auto is_common_node_H = [&](NodeID k){
                    if (H.edge(edge2_H).v1 == k or H.edge(edge2_H).v2 == k){
                        return true;
                    }
                    return false;
                };

                // Setting nodes k, l and m such that the path ijh is mapped to the path klm
                NodeID l, k; 
                if (is_common_node_H(H.edge(edge1_H).v1)) {
                    l = H.edge(edge1_H).v1;
                    k = H.edge(edge1_H).v2;
                } else {
                    assert(is_common_node_H(H.edge(edge1_H).v2));
                    l = H.edge(edge1_H).v2;
                    k = H.edge(edge1_H).v1;
                }
                NodeID m = (H.edge(edge2_H).v1 == l) ?
                        H.edge(edge2_H).v2 :
                        H.edge(edge2_H).v1;

                // Angle for path klm
                double angle2 = (params.angle_type == AngleType::Signed) ?
                        compute_signed_angle_between_lines(H.node(k).p, H.node(l).p, H.node(m).p) :
                        compute_unsigned_angle_between_lines(H.node(k).p, H.node(l).p, H.node(m).p);

                objective_value += params.cost_factor_angle_difference * std::abs(angle1 - angle2);
            }
        }
    }
    return objective_value;
}

} // end of anonymous namespace


MappingCost compute_mapping_cost(
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap,
        const ModelParameters &params)
{
    // Each sum receives the same additions in the same order as the former separate functions
    // (compute_length_deletion/insertion/diff_objective, compute_total_length_objective).
    MappingCost cost;
    EdgeID num_seen_edges = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_deletion()){
            EdgeID g = edge_pair.g.value();
            const double c = params.cost_factor_deletion_insertion * G.edge(g).length;
            cost.deletion += c;
            cost.edge_total += c;
            ++num_seen_edges;
        }
        else if (edge_pair.is_insertion()){
            EdgeID h = edge_pair.h.value();
            const double c = params.cost_factor_deletion_insertion * H.edge(h).length;
            cost.insertion += c;
            cost.edge_total += c;
            ++num_seen_edges;
        }
        else if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            EdgeID h = edge_pair.h.value();
            const double c = params.cost_factor_length_difference * std::abs(G.edge(g).length - H.edge(h).length);
            cost.length_difference += c;
            cost.edge_total += c;
            num_seen_edges += 2;
        }
    }
    assert(num_seen_edges == G.num_edges() + H.num_edges());
    cost.path_angle = path_angle_cost(params, G, H, edgemap);
    return cost;
}
