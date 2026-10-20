#include "distance/mapping_cost.h"

#include "utility/angle_computation.h"

#include <cassert>
#include <cmath>
#include <vector>

double compute_total_length_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    EdgeID num_seen_edges = 0;

    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_deletion()){
            EdgeID g = edge_pair.g.value();
            objective_value += params.cost_factor_deletion_insertion * G.edge(g).length;
            ++num_seen_edges;
        }
        else if (edge_pair.is_insertion()){
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_deletion_insertion * H.edge(h).length;
            ++num_seen_edges;
        }
        else if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_length_difference * std::abs(G.edge(g).length - H.edge(h).length);
            num_seen_edges += 2;
        }
    }
    assert(num_seen_edges == G.num_edges() + H.num_edges());
    return objective_value;
}

double compute_length_diff_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_length_difference * std::abs(G.edge(g).length - H.edge(h).length);
        }
    }
    return objective_value;
}

double compute_length_insertion_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_insertion()){
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_deletion_insertion * H.edge(h).length;
        }
    }
    return objective_value;
}

double compute_length_deletion_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_deletion()){
            EdgeID g = edge_pair.g.value();
            objective_value += params.cost_factor_deletion_insertion * G.edge(g).length;
        }
    }
    return objective_value;
}

double compute_total_angle_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    std::vector<std::vector<EdgeMap::EdgePair>> G_mapped_edge_adjacencies(G.num_vertices());

    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            G_mapped_edge_adjacencies[G.edge(g).v1].push_back(edge_pair);
            G_mapped_edge_adjacencies[G.edge(g).v2].push_back(edge_pair);
        }
    }

    double objective_value = 0;

    for (VertexID j = 0; j < G.num_vertices(); ++j){
        // If there are at least two mapped edges incident to j, 
        // get path mapping cost for all paths with j in the middle
        if (G_mapped_edge_adjacencies[j].size() < 2) continue;
        for (auto const &edge_pair_1 : G_mapped_edge_adjacencies[j]){
            assert(edge_pair_1.is_substitution());

            // First mapped edge pair in G and H
            EdgeID edge1_G = edge_pair_1.g.value();
            EdgeID edge1_H = edge_pair_1.h.value();

            // Other G-vertex adjacent to j
            VertexID i = G.edge(edge1_G).v1 == j ? 
                    G.edge(edge1_G).v2 :
                    G.edge(edge1_G).v1;

            for (auto const &edge_pair_2 : G_mapped_edge_adjacencies[j]){
                if (edge_pair_2 == edge_pair_1) continue;
                assert(edge_pair_2.is_substitution());
                
                // Second mapped edge (different from first edge)
                EdgeID edge2_G = edge_pair_2.g.value();
                EdgeID edge2_H = edge_pair_2.h.value();

                // Other G-vertex adjacent to j and different from i
                VertexID h = (G.edge(edge2_G).v1 == j) ? 
                        G.edge(edge2_G).v2 : 
                        G.edge(edge2_G).v1;

                // Angle for path ijh
                double angle1 = (params.angle_type == AngleType::Signed) ?
                        compute_signed_angle_between_lines(G.vertex(i).p, G.vertex(j).p, G.vertex(h).p) :
                        compute_unsigned_angle_between_lines(G.vertex(i).p, G.vertex(j).p, G.vertex(h).p);

                // Helper to find middle vertex of corresponding path in H
                auto is_common_vertex_H = [&](VertexID k){
                    if (H.edge(edge2_H).v1 == k or H.edge(edge2_H).v2 == k){
                        return true;
                    }
                    return false;
                };

                // Setting vertices k, l and m such that the path ijh is mapped to the path klm
                VertexID l, k; 
                if (is_common_vertex_H(H.edge(edge1_H).v1)) {
                    l = H.edge(edge1_H).v1;
                    k = H.edge(edge1_H).v2;
                } else {
                    assert(is_common_vertex_H(H.edge(edge1_H).v2));
                    l = H.edge(edge1_H).v2;
                    k = H.edge(edge1_H).v1;
                }
                VertexID m = (H.edge(edge2_H).v1 == l) ?
                        H.edge(edge2_H).v2 :
                        H.edge(edge2_H).v1;

                // Angle for path klm
                double angle2 = (params.angle_type == AngleType::Signed) ?
                        compute_signed_angle_between_lines(H.vertex(k).p, H.vertex(l).p, H.vertex(m).p) :
                        compute_unsigned_angle_between_lines(H.vertex(k).p, H.vertex(l).p, H.vertex(m).p);

                objective_value += params.cost_factor_angle_difference * std::abs(angle1 - angle2);
            }
        }
    }
    return objective_value;
}
