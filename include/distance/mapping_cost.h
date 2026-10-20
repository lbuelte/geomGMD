#ifndef GEOMGMD_DISTANCE_MAPPING_COST_H
#define GEOMGMD_DISTANCE_MAPPING_COST_H

// Cost of a given constrained edge map (paper, Eq. (2)-(4)), computed directly from the edge map
// (independently of the BLP), weighted with the cost factors in params.

#include "distance/parameters.h"
#include "utility/edgemap.h"
#include "utility/geometric_graph.h"

struct MappingCost {
    double deletion = 0;           // c_id * l(e) for the deleted edges of G
    double insertion = 0;          // c_id * l(e) for the inserted edges of H
    double length_difference = 0;  // c_m * |l(e) - l(psi(e))| for the mapped edges
    double edge_total = 0;         // deletion + insertion + length difference (summed in edge pair order)
    double path_angle = 0;         // c_r * |alpha(q) - alpha(rho(q))| for the mapped paths of length two

    double total() const { return edge_total + path_angle; }
};

// The edges of G and H in edgemap refer to G and H in this order.
MappingCost compute_mapping_cost(
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap,
        const ModelParameters &params);

#endif // GEOMGMD_DISTANCE_MAPPING_COST_H
