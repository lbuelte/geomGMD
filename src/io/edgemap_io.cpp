// Created by Laura Bülte on 2026-04-09

#include "io/edgemap_io.h"

#include "json/json.hpp"
#include "geometric_graph.h"
#include "geometry_helpers.h"

#include <fstream>
#include <iostream>
#include <set>
#include <unordered_map>
#include <cmath>

namespace{

struct RGB {
    int r, g, b;
};

RGB hsv_to_rgb(double h, double s, double v)
{
    double c = v * s;
    double x = c * (1 - std::fabs(std::fmod(h / 60.0, 2) - 1));
    double m = v - c;

    double r1, g1, b1;
    if (h < 60)      { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120){ r1 = x; g1 = c; b1 = 0; }
    else if (h < 180){ r1 = 0; g1 = c; b1 = x; }
    else if (h < 240){ r1 = 0; g1 = x; b1 = c; }
    else if (h < 300){ r1 = x; g1 = 0; b1 = c; }
    else             { r1 = c; g1 = 0; b1 = x; }

    return {
        static_cast<int>((r1 + m) * 255),
        static_cast<int>((g1 + m) * 255),
        static_cast<int>((b1 + m) * 255)
    };
}

std::vector<RGB> generate_distinct_colors(std::size_t n)
{
    std::vector<RGB> colors;
    colors.reserve(n);

    for (std::size_t i = 0; i < n; ++i) {
        double hue = std::fmod(360.0 * i * 0.61803398875, 360.0);
        colors.push_back(hsv_to_rgb(hue, 0.7, 0.9));
    }
    return colors;
}


struct BBox {
    double min_x, max_x, min_y, max_y;
};

BBox compute_bbox(const GeometricGraph& g) {
    BBox b;
    b.min_x = b.min_y = std::numeric_limits<double>::infinity();
    b.max_x = b.max_y = -std::numeric_limits<double>::infinity();

    for (const auto& v : g.vertices()) {
        b.min_x = std::min(b.min_x, v.p.x);
        b.min_y = std::min(b.min_y, v.p.y);
        b.max_x = std::max(b.max_x, v.p.x);
        b.max_y = std::max(b.max_y, v.p.y);
    }
    return b;
}

} // end of anonymous namespace

std::optional<json> find_graph_mapping_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id,
        bool &g_before_h) 
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            json entry = json::parse(line);

            if (entry.contains("G_id") && entry.contains("H_id")) {
                GraphID G = entry["G_id"];
                GraphID H = entry["H_id"];

                if ((G == g_id && H == h_id)){
                    g_before_h = true;                    
                    return entry;
                }
                if ((G == h_id && H == g_id)) {
                    g_before_h = false;                    
                    return entry;
                }
            }
        } catch (const json::parse_error&) {
            continue;
        }
    }
    return std::nullopt;
}


std::optional<double> find_modGED_value_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }
    std::string line;

    std::getline(file, line); // skip first line
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;

        std::string i_str, j_str, lb_str, ub_str, optimal_str;

        // Read columns (assuming comma-separated CSV)
        std::getline(ss, i_str, ',');
        std::getline(ss, j_str, ',');
        std::getline(ss, lb_str, ',');
        std::getline(ss, ub_str, ',');
        std::getline(ss, optimal_str, ',');

        // Convert i and j
        GraphID i = static_cast<GraphID>(std::stoll(i_str));
        GraphID j = static_cast<GraphID>(std::stoll(j_str));

        // Check unordered match
        if ((i == g_id && j == h_id) || (i == h_id && j == g_id)) {
            double ub = std::stod(ub_str);
            return ub;
        }
    }

    // Not found
    return std::nullopt;
}


EdgeMap::EdgeMap(json const &mapping_json, GeometricGraph const &G, GeometricGraph const &H) 
{
    // Map storing for each mapped node in g its partner in h
    std::unordered_map<VertexID, VertexID> g_to_h;
    for (const auto& entry : mapping_json["node_map"]) {
        if (!entry["g"].is_null() && !entry["h"].is_null()) {
            g_to_h[entry["g"]] = entry["h"];
        }
    }

    // Set storing the tuples of edge endpoints in H
    std::set<std::pair<VertexID, VertexID>> h_edges;
    for (auto const &edge : H.edges()) {
        auto a = std::min(edge.v1, edge.v2);
        auto b = std::max(edge.v1, edge.v2);
        h_edges.insert({a, b});
    }

    std::vector<bool> h_edge_is_mapped(H.num_edges(), false);

    std::vector<EdgePair> mapped_edges;
    for (EdgeID eg = 0; eg < G.num_edges(); ++eg) {
        auto const edge = G.edge(eg);

        if (g_to_h.contains(edge.v1) && g_to_h.contains(edge.v2)) {
            VertexID h1 = g_to_h[edge.v1];
            VertexID h2 = g_to_h[edge.v2];

            auto a = std::min(h1, h2);
            auto b = std::max(h1, h2);

            if (h_edges.contains({a, b})) {
                // find actual edge id in H
                for (EdgeID eh = 0; eh < H.num_edges(); ++eh) {
                    auto hedge = H.edge(eh);
                    if ((hedge.v1 == h1 && hedge.v2 == h2) ||
                        (hedge.v1 == h2 && hedge.v2 == h1)) {

                        mapped_edges.push_back({eg, eh});
                        h_edge_is_mapped[eh] = true;
                        break;
                    }
                }
            } else {
                // If the mapped nodes are not endpoints of an edge, e is deleted
                mapped_edges.push_back({eg, std::nullopt});
            }
        } else {
            // If not both endpoints of e are mapped, it is deleted
            mapped_edges.emplace_back(eg, std::nullopt);
        }
    }
    edge_pairs = std::move(mapped_edges);
    for (EdgeID eh = 0; eh < H.num_edges(); ++eh){
        if (not h_edge_is_mapped[eh]){
            edge_pairs.emplace_back(std::nullopt, eh);
        }
    }
}


void write_edge_mapping_svg(
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap &edgemap,
        SVGLegendInfo const &legend_info,
        const std::string& filename,
        double canvas_size)
{
    std::ofstream svg(filename);
    if (!svg.is_open() || !svg.good()) {
        throw std::runtime_error("ERROR: Could not open file for writing: " + filename);
    }

    // Margin and spacing between graphs
    double margin = 20.0;
    double gap = 40.0;

    // Compute bbox, height and width of both graphs
    BBox bG = compute_bbox(G);
    BBox bH = compute_bbox(H);
    double widthG = bG.max_x - bG.min_x;
    double widthH = bH.max_x - bH.min_x;
    double heightG = bG.max_y - bG.min_y;
    double heightH = bH.max_y - bH.min_y;

    // Available space
    double availW = canvas_size - 2 * margin - gap;
    double availH = canvas_size - 2 * margin;

    // Computing shared scaling factor
    double scaleX = availW / (widthG + widthH);
    double scaleY = availH / std::max(heightG, heightH);
    double scale = std::min(scaleX, scaleY);

    // Width of G and H scaled
    double pixelWidthG = widthG * scale;
    double pixelWidthH = widthH * scale;

    // Starting x-pos and staring y-pos
    double xG = margin;
    double xH = margin + pixelWidthG + gap;
    double canvasBottom = canvas_size - margin;

    auto draw_graph = [&](const GeometricGraph& g,
                          const BBox& b,
                          double x_offset,
                          const std::map<EdgeID, RGB>& edge_colors,
                          const std::string& name)
    {
        auto sx = [&](double x) {
            return (x - b.min_x) * scale + x_offset;
        };
        auto sy = [&](double y) {
            return canvas_size - margin - (y - b.min_y) * scale;
        };

        for (size_t e = 0; e < g.num_edges(); ++e) {
            auto edge = g.edge(e);

            auto p1 = g.vertex(edge.v1).p;
            auto p2 = g.vertex(edge.v2).p;

            std::string color = "lightgray";

            if (edge_colors.contains(e)) {
                const auto& c = edge_colors.at(e);
                color = "rgb(" + std::to_string(c.r) + "," +
                                std::to_string(c.g) + "," +
                                std::to_string(c.b) + ")";
            }

            svg << "<line x1='" << sx(p1.x)
                << "' y1='" << sy(p1.y)
                << "' x2='" << sx(p2.x)
                << "' y2='" << sy(p2.y)
                << "' style='stroke:" << color
                << ";stroke-width:2'/>\n";
        }

        for (const auto& v : g.vertices()) {
            svg << "<circle cx='" << sx(v.p.x)
                << "' cy='" << sy(v.p.y)
                << "' r='2' fill='black'/>\n";
        }
    };

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='" << canvas_size
        << "' height='" << canvas_size
        << "' viewBox='0 0 " << canvas_size << " " << canvas_size << "'>\n";

    svg << "<rect x='0' y='0' width='" << canvas_size
        << "' height='" << canvas_size
        << "' style='fill:white'/>\n";

    auto colors = generate_distinct_colors(edgemap.edge_pairs.size());
    std::map<EdgeID, RGB> colors_G, colors_H;
    RGB lightgray = {83, 83, 83};

    for (size_t i = 0; i < edgemap.edge_pairs.size(); ++i) {
        if (edgemap.edge_pairs[i].is_substitution()){
            colors_G[edgemap.edge_pairs[i].g.value()] = colors[i];
            colors_H[edgemap.edge_pairs[i].h.value()] = colors[i];
        }
        else if (edgemap.edge_pairs[i].is_insertion()){
            colors_H[edgemap.edge_pairs[i].h.value()] = lightgray;
        }
        else if (edgemap.edge_pairs[i].is_deletion()){
            colors_G[edgemap.edge_pairs[i].g.value()] = lightgray;
        }
    }

    draw_graph(G, bG, xG, colors_G, "G");
    draw_graph(H, bH, xH, colors_H, "H");


    // -------- Legend ----------- //

    // Graph IDs
    std::string graph_ids = std::to_string(legend_info.g) + " - " + std::to_string(legend_info.h);
  
    // Optimality and distance
    std::string optimality = legend_info.map_is_optimal ? "optimal map" : "best found map";
    std::string distance_text = [&](){ 
        if (legend_info.modGED_dist.has_value()){
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(3) << legend_info.modGED_dist.value();
            std::string distance_str = ss.str();
            return "distance: " + distance_str;
        }
        return std::string("distance: /");
    }();
    std::string length_diff_obj_text = [&](){
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << compute_length_diff_objective(legend_info.params, G, H, edgemap);
        std::string length_str = ss.str();
        return "edge length diff: " + length_str;
    }();
    std::string insertion_obj_text = [&](){
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << compute_length_insertion_objective(legend_info.params, G, H, edgemap);
        std::string length_str = ss.str();
        return "edge insertion: " + length_str;
    }();
    std::string deletion_obj_text = [&](){
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << compute_length_deletion_objective(legend_info.params, G, H, edgemap);
        std::string length_str = ss.str();
        return "edge deletion: " + length_str;
    }();
    std::string angle_obj_text = [&](){
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(3) << compute_total_angle_objective(legend_info.params, G, H, edgemap);
        std::string angle_str = ss.str();
        return "angle diff: " + angle_str;
    }();

    // First, compute legend height, width and placement

    auto text_width = [](const std::string& s) {
        return 7.0 * s.size(); // approx for font-size 14
    };
    double w1 = text_width(optimality);
    double w2 = text_width(distance_text);
    double w3 = 100.0; // scale bar width
    double w4 = text_width(legend_info.dataset);
    double w5 = text_width(length_diff_obj_text);
    double w6 = text_width(angle_obj_text);
    double w7 = text_width(deletion_obj_text);
    double w8 = text_width(insertion_obj_text);

    double legend_width = std::max({w1, w2, w3, w4, w5, w6, w7, w8}) + 10; // padding
    
    double line_height = 16;

    double legend_height =
        9 * line_height +   // 2 text + scale label spacing
        10 +                // gap before scale bar
        6;                  // bar thickness area

    double pixelHeightG = heightG * scale;
    double availableHeight = canvas_size - 2 * margin;
    bool G_too_tall = pixelHeightG > 0.9 * availableHeight;
    bool place_right = G_too_tall;
    
    double legend_x = place_right
        ? (canvas_size - margin - legend_width)
        : (margin + 5);
    double legend_y = 30;

    // Draw background
    svg << "<rect x='" << legend_x - 5
        << "' y='" << legend_y - 14
        << "' width='" << legend_width
        << "' height='" << legend_height
        << "' fill='lightgray' opacity='1.0' />\n";

    // Draw text
    svg << "<text x='" << legend_x
        << "' y='" << legend_y
        << "' font-size='14' font-family='Arial' fill='black'>"
        << legend_info.dataset
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << graph_ids
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 2*line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << optimality
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 3*line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << distance_text
        << "</text>\n";
    
    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 4*line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << angle_obj_text
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 5*line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << length_diff_obj_text
        << "</text>\n";

    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 6*line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << deletion_obj_text
        << "</text>\n";
        
    svg << "<text x='" << legend_x
        << "' y='" << (legend_y + 7*line_height)
        << "' font-size='14' font-family='Arial' fill='black'>"
        << insertion_obj_text
        << "</text>\n";

    // Draw Scale 
    double bar_px = 100.0;
    double bar_m = bar_px / scale;
    double lx = legend_x;
    double ly = legend_y + 8.5 * line_height;

    svg << "<line x1='" << lx
        << "' y1='" << ly
        << "' x2='" << lx + bar_px
        << "' y2='" << ly
        << "' stroke='black' stroke-width='2'/>\n";

    svg << "<text x='" << lx
        << "' y='" << (ly - 6)
        << "' font-size='12'>"
        << bar_m << " m"
        << "</text>\n";

    svg << "</svg>";
}

double compute_total_length_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    EdgeID num_seen_edges = 0;

    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_deletion()){
            EdgeID g = edge_pair.g.value();
            objective_value += params.cost_factor_deletion_insertion * G.edge(g).length;
            ++num_seen_edges;
        }
        else if (edge_pair.is_insertion()){
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_deletion_insertion * H.edge(h).length;
            ++num_seen_edges;
        }
        else if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_length_difference * std::abs(G.edge(g).length - H.edge(h).length);
            num_seen_edges += 2;
        }
    }
    assert(num_seen_edges == G.num_edges() + H.num_edges());
    return objective_value;
}

double compute_length_diff_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_length_difference * std::abs(G.edge(g).length - H.edge(h).length);
        }
    }
    return objective_value;
}

double compute_length_insertion_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_insertion()){
            EdgeID h = edge_pair.h.value();
            objective_value += params.cost_factor_deletion_insertion * H.edge(h).length;
        }
    }
    return objective_value;
}

double compute_length_deletion_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    double objective_value = 0;
    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_deletion()){
            EdgeID g = edge_pair.g.value();
            objective_value += params.cost_factor_deletion_insertion * G.edge(g).length;
        }
    }
    return objective_value;
}

double compute_total_angle_objective(
        const GurobiParams &params,
        const GeometricGraph& G,
        const GeometricGraph& H,
        const EdgeMap& edgemap)
{
    std::vector<std::vector<EdgeMap::EdgePair>> G_mapped_edge_adjacencies(G.num_vertices());

    for (auto const &edge_pair : edgemap.edge_pairs){
        if (edge_pair.is_substitution()){
            EdgeID g = edge_pair.g.value();
            G_mapped_edge_adjacencies[G.edge(g).v1].push_back(edge_pair);
            G_mapped_edge_adjacencies[G.edge(g).v2].push_back(edge_pair);
        }
    }

    double objective_value = 0;

    for (VertexID j = 0; j < G.num_vertices(); ++j){
        // If there are at least two mapped edges incident to j, 
        // get path mapping cost for all paths with j in the middle
        if (G_mapped_edge_adjacencies[j].size() < 2) continue;
        for (auto const &edge_pair_1 : G_mapped_edge_adjacencies[j]){
            assert(edge_pair_1.is_substitution());

            // First mapped edge pair in G and H
            EdgeID edge1_G = edge_pair_1.g.value();
            EdgeID edge1_H = edge_pair_1.h.value();

            // Other G-vertex adjacent to j
            VertexID i = G.edge(edge1_G).v1 == j ? 
                    G.edge(edge1_G).v2 :
                    G.edge(edge1_G).v1;

            for (auto const &edge_pair_2 : G_mapped_edge_adjacencies[j]){
                if (edge_pair_2 == edge_pair_1) continue;
                assert(edge_pair_2.is_substitution());
                
                // Second mapped edge (different from first edge)
                EdgeID edge2_G = edge_pair_2.g.value();
                EdgeID edge2_H = edge_pair_2.h.value();

                // Other G-vertex adjacent to j and different from i
                VertexID h = (G.edge(edge2_G).v1 == j) ? 
                        G.edge(edge2_G).v2 : 
                        G.edge(edge2_G).v1;

                // Angle for path ijh
                double angle1 = (params.angle_type == AngleType::Signed) ?
                        compute_signed_angle_between_lines(G.vertex(i).p, G.vertex(j).p, G.vertex(h).p) :
                        compute_unsigned_angle_between_lines(G.vertex(i).p, G.vertex(j).p, G.vertex(h).p);

                // Helper to find middle vertex of corresponding path in H
                auto is_common_vertex_H = [&](VertexID k){
                    if (H.edge(edge2_H).v1 == k or H.edge(edge2_H).v2 == k){
                        return true;
                    }
                    return false;
                };

                // Setting vertices k, l and m such that the path ijh is mapped to the path klm
                VertexID l, k; 
                if (is_common_vertex_H(H.edge(edge1_H).v1)) {
                    l = H.edge(edge1_H).v1;
                    k = H.edge(edge1_H).v2;
                } else {
                    assert(is_common_vertex_H(H.edge(edge1_H).v2));
                    l = H.edge(edge1_H).v2;
                    k = H.edge(edge1_H).v1;
                }
                VertexID m = (H.edge(edge2_H).v1 == l) ?
                        H.edge(edge2_H).v2 :
                        H.edge(edge2_H).v1;

                // Angle for path klm
                double angle2 = (params.angle_type == AngleType::Signed) ?
                        compute_signed_angle_between_lines(H.vertex(k).p, H.vertex(l).p, H.vertex(m).p) :
                        compute_unsigned_angle_between_lines(H.vertex(k).p, H.vertex(l).p, H.vertex(m).p);

                objective_value += params.cost_factor_angle_difference * std::abs(angle1 - angle2);
            }
        }
    }
    return objective_value;
}