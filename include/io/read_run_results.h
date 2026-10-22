#ifndef GEOMGMD_IO_READ_RUN_RESULTS_H
#define GEOMGMD_IO_READ_RUN_RESULTS_H

// Reading the result files of a distance matrix run (edgemaps.json, intervaldistance_matrix.csv).

#include "distance/geomGMDSolver.h"
#include "utility/edgemap.h"
#include "utility/graph_base.h"

#include <filesystem>
#include <map>
#include <utility>

// Unordered pair of graph ids, stored as (smaller id, larger id)
using GraphPair = std::pair<GraphID, GraphID>;
GraphPair graph_pair(GraphID g, GraphID h);

// An edge map from edgemaps.json with the optimality flag of its computation
struct StoredEdgeMap {
    EdgeMap edge_map;
    bool optimal;
};

// All edge maps of an edgemaps.json file (one JSON object per line). Throws if the file cannot be read
// or a line is not a valid edge map.
std::map<GraphPair, StoredEdgeMap> read_edgemaps_file(std::filesystem::path const &path);

// All distances of an intervaldistance_matrix.csv file; pairs without a distance (nan) are left out.
// Throws if the file cannot be read or a line is malformed.
std::map<GraphPair, IntervalDistance> read_distance_matrix_file(std::filesystem::path const &path);

#endif // GEOMGMD_IO_READ_RUN_RESULTS_H
