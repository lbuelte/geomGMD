// Created by Laura Bülte on 2026-04-15.

#ifndef GEOMGMD_IO_EDGEMAP_IO_H
#define GEOMGMD_IO_EDGEMAP_IO_H

#include "utility/geometric_graph.h"

#include "config.h"
#include "distance/geomGMDSolver.h"

#include "json/json.hpp"
#include <optional>

using json = nlohmann::json;

// JSON representation of an edge map, as written to edgemaps.json:
//   {"G_id": <graph id of G>, "H_id": <graph id of H>, "optimal": <bool>,
//    "edge_map": [{"g": <edge of G or null>, "h": <edge of H or null>}, ...]}
json edgemap_to_json(EdgeMap const &edgemap, bool optimal);
EdgeMap edgemap_from_json(json const &j);

// JSON of the edge map computed by the solver, with the optimality of that run.
json edgemap_json(GeomGMDForiSolver &solver);


std::optional<json> find_graph_mapping_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id,
        bool &g_before_h);

std::optional<double> find_geomGMD_value_from_file(
        const std::string& filename,
        GraphID g_id,
        GraphID h_id);


#endif // GEOMGMD_IO_EDGEMAP_IO_H