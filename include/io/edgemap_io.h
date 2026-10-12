// Created by Laura Bülte on 2026-04-15.

#ifndef POLYGONCLUSTERING_EDGEMAP_IO_H
#define POLYGONCLUSTERING_EDGEMAP_IO_H

#include "json/json.hpp"
#include "geometric_graph.h"
#include "config.h"
#include "geomGMDSolver.h"

#include <optional>

using json = nlohmann::json;


class EdgeMap {

public:

    struct EdgePair {
        std::optional<EdgeID> g;
        std::optional<EdgeID> h;

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

        bool is_substitution() const { return g.has_value() and h.has_value(); }
        bool is_insertion() const { return not g.has_value() and h.has_value(); }
        bool is_deletion() const { return g.has_value() and not h.has_value(); }
    };

    EdgeMap(json const &mapping_json, GeometricGraph const &G, GeometricGraph const &H);

    std::vector<EdgePair> edge_pairs;

};

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


#endif // POLYGONCLUSTERING_EDGEMAP_IO_H