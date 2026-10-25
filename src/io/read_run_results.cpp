#include "io/read_run_results.h"

#include "io/edgemap_io.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

std::ifstream open_result_file(std::filesystem::path const &path)
{
    std::ifstream file(path);
    if (not file.is_open()) {
        throw std::runtime_error("Could not open " + path.string()
                                 + " (written by geomGMD_matrix with the same dataset, output_path and run_name)");
    }
    return file;
}

std::runtime_error format_error(std::filesystem::path const &path, size_t line_number, std::string const &what)
{
    return std::runtime_error("Invalid line " + std::to_string(line_number) + " in " + path.string() + ": " + what);
}

} // end of anonymous namespace


GraphPair graph_pair(GraphID g, GraphID h)
{
    return {std::min(g, h), std::max(g, h)};
}

std::map<GraphPair, StoredEdgeMap> read_edgemaps_file(std::filesystem::path const &path)
{
    auto file = open_result_file(path);
    std::map<GraphPair, StoredEdgeMap> edgemaps;
    std::string line;
    size_t line_number = 0;
    while (std::getline(file, line)) {
        ++line_number;
        if (line.empty()) continue;
        try {
            json const entry = json::parse(line);
            EdgeMap edgemap = edgemap_from_json(entry);
            bool const optimal = entry.at("optimal").get<bool>();
            if (not edgemap.g_graph_id or not edgemap.h_graph_id) {
                throw std::runtime_error("edge map without graph ids");
            }
            GraphPair const key = graph_pair(*edgemap.g_graph_id, *edgemap.h_graph_id);
            edgemaps.insert_or_assign(key, StoredEdgeMap{std::move(edgemap), optimal});
        } catch (std::exception const &e) {
            throw format_error(path, line_number, e.what());
        }
    }
    return edgemaps;
}

std::map<GraphPair, IntervalDistance> read_distance_matrix_file(std::filesystem::path const &path)
{
    auto file = open_result_file(path);
    std::map<GraphPair, IntervalDistance> distances;
    std::string line;
    std::getline(file, line);  // header row,col,lb,ub,optimal
    size_t line_number = 1;
    while (std::getline(file, line)) {
        ++line_number;
        if (line.empty()) continue;
        std::vector<std::string> cells;
        std::stringstream ss(line);
        for (std::string cell; std::getline(ss, cell, ',');) cells.push_back(cell);
        if (cells.size() != 5) {
            throw format_error(path, line_number, "expected 5 columns row,col,lb,ub,optimal");
        }
        try {
            GraphID const row = std::stoull(cells[0]);
            GraphID const col = std::stoull(cells[1]);
            double const lb = std::stod(cells[2]);
            double const ub = std::stod(cells[3]);
            if (std::isnan(lb) or std::isnan(ub)) continue;  // pair without a distance
            distances.insert_or_assign(graph_pair(row, col), IntervalDistance(lb, ub));
        } catch (std::exception const &) {
            throw format_error(path, line_number, "not a number");
        }
    }
    return distances;
}
