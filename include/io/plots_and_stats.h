#ifndef POLYGONCLUSTERING_PLOTS_AND_STATS_H
#define POLYGONCLUSTERING_PLOTS_AND_STATS_H

#include <gurobi_c++.h>

#include "geomGMDSolver.h"
#include "utility/log.h"

#include <filesystem>
#include <regex>
#include <cmath>

using ClusterID = size_t;

namespace{

const std::vector<std::string> cluster_colors = {
    "rgba(183, 183, 229, 1)",       
    "rgb(192, 230, 222)",      
    "rgb(240,217,147)",        
    "rgba(145, 194, 228, 1)",      
    "rgba(195, 228, 153, 1)",        
    "rgba(226, 154, 126, 1)",       
    "rgba(234, 161, 172, 1)",       
    "rgb(186,172,189)",
    "rgba(146, 189, 150, 1)",
    "rgba(229, 236, 158, 1)",
    "rgba(209, 180, 233, 1)",
    "rgba(243, 178, 229, 1)",
    "rgba(173, 229, 243, 1)",
    "rgba(247, 211, 181, 1)",
    "rgba(255, 200, 200, 1)"    
};


void delete_svgs(const std::string& folderPath) {
    try {
        if (!std::filesystem::exists(folderPath)) {
            geomgmd::log::error("Directory does not exist: ", folderPath);
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".svg") {
                std::filesystem::remove(entry.path());
            }
        }

        geomgmd::log::info("All .svg files deleted in ", folderPath);
    }
    catch (const std::exception& e) {
        geomgmd::log::error("Error while deleting .svg files: ", e.what());
    }
}


void write_instance_and_cluster_svg(GeometricGraph const &g, ClusterID c,
        const std::string &filename, double padding = 10, double canvas_size = 1000.0)
{
    double radius = 2.0; // fixed small radius for visualization

    if (g.num_vertices() == 0) {
        return;
    }

    // ---- bounding box ----
    double min_x = g.vertex(0).p.x;
    double min_y = g.vertex(0).p.y;
    double max_x = min_x;
    double max_y = min_y;

    for (const auto& v : g.vertices()) {
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
    const auto bg_color = c < cluster_colors.size() ? cluster_colors[c] : "lightgrey";

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << canvas_size
        << "' height='" << canvas_size
        << "' viewBox='0 0 " << canvas_size << " " << canvas_size << "'>\n";

    svg << "<rect x='0' y='0' width='" << canvas_size
        << "' height='" << canvas_size
        << "' style='fill:" << bg_color << "'/>\n";

    // ---- draw medial axis edges ----
    for (size_t k = 0; k < g.num_edges(); ++k) {
        int i1 = g.edge(k).v1;
        int i2 = g.edge(k).v2;
        const auto& p1 = g.vertex(i1).p;
        const auto& p2 = g.vertex(i2).p;

        svg << "<line x1='" << sx(p1.x) << "' y1='" << sy(p1.y)
            << "' x2='" << sx(p2.x) << "' y2='" << sy(p2.y)
            << "' style='stroke:green;stroke-width:2'/>\n";
        svg << "'/>\n";

    }

    // ---- draw medial axis vertices ----
    for (const auto& v : g.vertices()) {
        svg << "<circle cx='" << sx(v.p.x) << "' cy='" << sy(v.p.y)
            << "' r='" << radius << "' fill='black' stroke='black' stroke-width='2'/>\n";
    }

    double legend_x = 15;
    double legend_y = 25;

    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Vertices: " << g.num_vertices()
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 18)
        << "' font-family='Arial' font-size='14' fill='black'>"
        << "Edges: " << g.num_edges()
        << "</text>\n";

        svg << "</svg>\n";
        svg.close();
}


void write_interval_distance_matrix(
        std::vector<std::vector<IntervalDistance>> const &distance_matrix, 
        std::string const &filename){

    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        geomgmd::log::error("Error: Could not open ", filename);
        return;
    }

    const int width = 10;        // column width
    const int precision = 2;    // decimal precision

    const size_t n = distance_matrix.size();

    ofs << std::fixed << std::setprecision(precision);

    // Header row
    ofs << std::setw(5) << " ";
    for (size_t j = 0; j < n; ++j)
        ofs << std::setw(width + 1) << j;
    ofs << "\n";

    // Top border
    ofs << std::setw(6) << " " << "+";
    for (size_t j = 0; j < n; ++j)
        ofs << std::string(width, '-') << "+";
    ofs << "\n";

    // Each row with left index and vertical separators
    for (size_t i = 0; i < n; ++i) {
        ofs << std::setw(5) << i << " |";
        for (size_t j = 0; j < n; ++j)
            ofs << std::setw(width - 1) << distance_matrix[i][j].lb << " - " << distance_matrix[i][j].ub << " |";
        ofs << "\n";
    }

    // Bottom border
    ofs << std::setw(6) << " " << "+";
    for (size_t j = 0; j < n; ++j)
        ofs << std::string(width, '-') << "+";
    ofs << "\n";

    ofs.close();
    geomgmd::log::info("Wrote matrix to ", filename);            
}


std::vector<std::vector<IntervalDistance>> read_interval_distance_matrix(
        std::string const &filename)
{
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        geomgmd::log::error("Error: Could not open ", filename, " for reading");
        return {};
    }
    
    std::string line;

    // Skip header row
    std::getline(ifs, line);

    // Skip top border
    std::getline(ifs, line);

    std::vector<std::vector<IntervalDistance>> matrix;

    while (std::getline(ifs, line)) {

        // Stop if bottom border reached
        if (line.find('+') != std::string::npos && line.find('-') != std::string::npos)
            break;

        std::stringstream ss(line);

        size_t row_index;
        char pipe;

        ss >> row_index >> pipe; // read "i |"

        std::vector<IntervalDistance> row;

        while (ss) {
            IntervalDistance d {-1, -1};
            char dash;

            ss >> d.lb;
            if (!ss) break;

            ss >> dash; // '-'
            ss >> d.ub;
            ss >> pipe; // '|'

            row.push_back(d);
        }

        matrix.push_back(row);
    }

    ifs.close();
    return matrix;
}

} //end of namespace

#endif //POLYGONCLUSTERING_PLOTS_AND_STATS_H