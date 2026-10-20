// Created by Laura Bülte on 2026-04-15.

#ifndef GEOMGMD_IO_EDGEMAP_IO_H
#define GEOMGMD_IO_EDGEMAP_IO_H

#include "utility/geometric_graph.h"

#include "config.h"
#include "distance/geomGMDSolver.h"

#include "json/json.hpp"
#include <optional>

using json = nlohmann::json;

// JSON representation of an edge map, as written to edgemaps.json:
//   {"G_id": <graph id of G>, "H_id": <graph id of H>, "optimal": <bool>,
//    "edge_map": [{"g": <edge of G or null>, "h": <edge of H or null>}, ...]}
json edgemap_to_json(EdgeMap const &edgemap, bool optimal);
EdgeMap edgemap_from_json(json const &j);

// JSON of the edge map computed by the solver, with the optimality of that run.
json edgemap_json(GeomGMDForiSolver &solver);

struct SVGLegendInfo {
    bool map_is_optimal;
    std::string dataset;
    GraphID g;
    GraphID h;
    GurobiParams const &params;
    std::optional<double> geomGMD_dist = std::nullopt;
};

std::optional<json> find_graph_mapping_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id,
        bool &g_before_h);

std::optional<double> find_geomGMD_value_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id);

void write_edge_mapping_svg(
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap,
        const SVGLegendInfo &legend_info,
        const std::string& filename,
        double canvas_size = 1000.0);

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


#endif // GEOMGMD_IO_EDGEMAP_IO_H