// Created by Laura Bülte on 2026-04-15.

#ifndef GEOMGMD_EDGEMAP_IO_H
#define GEOMGMD_EDGEMAP_IO_H

#include "utility/geometric_graph.h"

#include "config.h"
#include "geomGMDSolver.h"

#include "json/json.hpp"
#include <optional>

using json = nlohmann::json;

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


#endif // GEOMGMD_EDGEMAP_IO_H