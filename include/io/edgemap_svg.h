#ifndef GEOMGMD_IO_EDGEMAP_SVG_H
#define GEOMGMD_IO_EDGEMAP_SVG_H

// SVG plot of an edge map between two geometric graphs (mapped edges in matching colours, deleted and
// inserted edges in light gray), with a legend containing the cost breakdown.

#include "distance/geomGMDSolver.h"
#include "utility/edgemap.h"
#include "utility/geometric_graph.h"

#include <optional>
#include <string>

struct SVGLegendInfo {
    bool map_is_optimal;
    std::string dataset;
    GraphID g;
    GraphID h;
    ModelParameters params;
    std::optional<double> geomGMD_dist = std::nullopt;
};

void write_edge_mapping_svg(
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap,
        const SVGLegendInfo &legend_info,
        const std::string& filename,
        double canvas_size = 1000.0);

#endif // GEOMGMD_IO_EDGEMAP_SVG_H
