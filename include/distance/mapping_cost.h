#ifndef GEOMGMD_DISTANCE_MAPPING_COST_H
#define GEOMGMD_DISTANCE_MAPPING_COST_H

// Cost of a given constrained edge map (paper, Eq. (2)-(4)), computed directly from the edge map
// (independently of the BLP). The deletion/insertion and length difference costs and the path (angle)
// mapping costs are weighted with the cost factors in params.

#include "distance/geomGMDSolver.h"
#include "utility/edgemap.h"
#include "utility/geometric_graph.h"

// Edge deletion, insertion and edge mapping (length difference) costs
double compute_total_length_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap);

double compute_length_diff_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap);

double compute_length_insertion_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap);

double compute_length_deletion_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap);

double compute_total_angle_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap);

#endif // GEOMGMD_DISTANCE_MAPPING_COST_H
