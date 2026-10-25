#ifndef GEOMGMD_CLUSTERING_PLOTS_AND_STATS_H
#define GEOMGMD_CLUSTERING_PLOTS_AND_STATS_H

// SVG plots of a clustering: one SVG per graph, coloured by its cluster.

#include "utility/geometric_graph.h"

#include <cstddef>
#include <string>

using ClusterID = size_t;

// Deletes all .svg files in the folder (used before writing the plots of a new clustering).
void delete_svgs(const std::string& folderPath);

// SVG of graph g in the colour of cluster c.
void write_instance_and_cluster_svg(GeometricGraph const &g, ClusterID c,
        const std::string &filename, double padding = 10, double canvas_size = 1000.0);

#endif // GEOMGMD_CLUSTERING_PLOTS_AND_STATS_H