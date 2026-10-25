#ifndef GEOMGMD_IO_EDGEMAP_IO_H
#define GEOMGMD_IO_EDGEMAP_IO_H

#include "utility/geometric_graph.h"

#include "config.h"
#include "distance/geomGMD.h"

#include "json/json.hpp"
#include <optional>

// JSON representation of an edge map, as written to edgemaps.json:
//   {"G_id": <graph id of G or null>, "H_id": <graph id of H or null>, "optimal": <bool>,
//    "edge_map": [{"g": <edge of G or null>, "h": <edge of H or null>}, ...]}
nlohmann::json edgemap_to_json(EdgeMap const &edgemap, bool optimal);
EdgeMap edgemap_from_json(nlohmann::json const &j);

// JSON of the edge map of a computed pair, with the optimality of that run.
nlohmann::json edgemap_json(GeomGMDResult const &result);


#endif // GEOMGMD_IO_EDGEMAP_IO_H