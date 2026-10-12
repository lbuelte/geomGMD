#ifndef GPKG_IO_H
#define GPKG_IO_H

#include <ogrsf_frmts.h>
#include <vector>
#include <string>
#include <iostream>
#include <filesystem>

#include "geometric_graph.h"

void assignClustersToPolygons(
        const std::string& inputGpkg,
        const std::string& outputGpkg,
        const std::unordered_map<PolygonID, GraphID> &polygon_id_to_graph_id,
        const std::vector<int>& clusterIds,
        const std::vector<int>& medoidInfo,
        bool reduce_to_assigned_polys = true);

#endif //GPKG_IO_H
