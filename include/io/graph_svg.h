#ifndef GEOMGMD_IO_GRAPH_SVG_H
#define GEOMGMD_IO_GRAPH_SVG_H

// SVG plot of a single geometric graph (for debugging): edges in green, nodes in black,
// with a legend containing the number of nodes and edges.

#include "utility/geometric_graph.h"

#include <string>

// Nothing is written if the graph is empty or its bounding box has zero width or height.
void write_graph_svg(const GeometricGraph& G,
                     const std::string& filename = "geometric_graph.svg",
                     double padding = 10, double canvas_size = 1000.0);

#endif // GEOMGMD_IO_GRAPH_SVG_H
