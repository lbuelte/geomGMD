#include "io/graph_svg.h"

#include <algorithm>
#include <cstddef>
#include <fstream>

void write_graph_svg(const GeometricGraph& G, const std::string &filename, double padding, double canvas_size)
{
    double radius = 2.0; // fixed small radius for visualization

    if (G.num_vertices() == 0) {
        return;
    }

    // ---- bounding box ----
    double min_x = G.vertex(0).p.x;
    double min_y = G.vertex(0).p.y;
    double max_x = min_x;
    double max_y = min_y;

    for (const auto& v : G.vertices()) {
        min_x = std::min(min_x, v.p.x);
        min_y = std::min(min_y, v.p.y);
        max_x = std::max(max_x, v.p.x);
        max_y = std::max(max_y, v.p.y);
    }

    double width  = max_x - min_x;
    double height = max_y - min_y;

    if (width == 0 || height == 0) return;

    // ---- compute scaling factor ----
    double scale_x = (canvas_size - 2*padding) / width;
    double scale_y = (canvas_size - 2*padding) / height;
    double scale = std::min(scale_x, scale_y);

    // ---- helper lambda to scale coordinates ----
    auto sx = [&](double x) { return (x - min_x) * scale + padding; };
    auto sy = [&](double y) { return canvas_size - ((y - min_y) * scale + padding); }; // flip Y

    std::ofstream svg(filename);
    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << canvas_size
        << "' height='" << canvas_size
        << "' viewBox='0 0 " << canvas_size << " " << canvas_size << "'>\n";

    svg << "<rect x='0' y='0' width='" << canvas_size
        << "' height='" << canvas_size
        << "' style='fill:white'/>\n";

    // ---- draw medial axis edges ----
    for (const auto& e : G.edges()) {
        const auto& p1 = G.vertex(e.v1).p;
        const auto& p2 = G.vertex(e.v2).p;

        svg << "<line x1='" << sx(p1.x) << "' y1='" << sy(p1.y)
            << "' x2='" << sx(p2.x) << "' y2='" << sy(p2.y)
            << "' style='stroke:green;stroke-width:2'/>\n";
        svg << "'/>\n";

    }

    // ---- draw medial axis vertices ----
    for (const auto& v : G.vertices()) {
        svg << "<circle cx='" << sx(v.p.x) << "' cy='" << sy(v.p.y)
            << "' r='" << radius << "' fill='black' stroke='black' stroke-width='2'/>\n";
    }

    // ---- legend with graph size ----
    std::size_t num_vertices = G.num_vertices();
    std::size_t num_edges    = G.num_edges();

    double legend_x = 15;
    double legend_y = 25;

    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Vertices: " << num_vertices
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 18)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Edges: " << num_edges
        << "</text>\n";

    svg << "</svg>\n";
    svg.close();
}
