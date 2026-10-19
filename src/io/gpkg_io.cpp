#include "io/gpkg_io.h"
#include "utility/log.h"

void assignClustersToPolygons(
        const std::string& inputGpkg,
        const std::string& outputGpkg,
        const std::unordered_map<PolygonID, GraphID> &polygon_id_to_graph_id,
        const std::vector<int>& clusterIds,
        const std::vector<int>& medoidInfo,
        bool reduce_to_assigned_polys)
{
    GDALAllRegister();

    // Open existing geopackage
    GDALDataset* inDs = (GDALDataset*) GDALOpenEx(
            inputGpkg.c_str(),
            GDAL_OF_VECTOR | GDAL_OF_READONLY,
            nullptr, nullptr, nullptr);

    if (!inDs) {
        geomgmd::log::error("Failed to open input GeoPackage: ", inputGpkg);
        return;
    }

    OGRLayer* inLayer = nullptr;
    try {
        inLayer = inDs->GetLayerByName("polygons");
        if (!inLayer) {
            geomgmd::log::warning("Layer 'polygons' not found, trying layer 0");
            inLayer = inDs->GetLayer(0);
        }
        if (!inLayer) {
            throw std::runtime_error("Could not get any layer from GeoPackage.");
        }

        geomgmd::log::info("Layer name: ", inLayer->GetName());
        geomgmd::log::info("Feature count: ", inLayer->GetFeatureCount());
        OGRFeatureDefn* layerDef = inLayer->GetLayerDefn();
        geomgmd::log::info("Geometry type: ", OGRGeometryTypeToName(layerDef->GetGeomType()));

    } catch (const std::exception& e) {
        geomgmd::log::error("Error: ", e.what());
        GDALClose(inDs);
        return;
    }

    // Create output geopackage
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if (!driver) {
        geomgmd::log::error("GPKG driver not available");
        GDALClose(inDs);
        return;
    }

    // Ensure output directory exists
    std::filesystem::path outPath(outputGpkg);
    std::filesystem::create_directories(outPath.parent_path());

    // Overwrite if exists
    driver->Delete(outputGpkg.c_str());

    GDALDataset* outDs = driver->Create(
            outputGpkg.c_str(), 0, 0, 0, GDT_Unknown, nullptr);

    if (!outDs) {
        geomgmd::log::error("Failed to create output GeoPackage");
        GDALClose(inDs);
        return;
    }

    // Clone spatial ref
    OGRSpatialReference* srs = inLayer->GetSpatialRef();

    // Create output layer
    OGRLayer* outLayer = outDs->CreateLayer(
            "polygons", srs, wkbPolygon, nullptr);

    // Copy fields from input layer
    OGRFeatureDefn* inDef = inLayer->GetLayerDefn();
    for (int i = 0; i < inDef->GetFieldCount(); i++) {
        outLayer->CreateField(inDef->GetFieldDefn(i));
    }

    // Add new "cluster" field
    OGRFieldDefn clusterField("cluster", OFTInteger);
    outLayer->CreateField(&clusterField);

    // Add new "isMedoid" field
    OGRFieldDefn medoidField("medoid", OFTInteger);
    outLayer->CreateField(&medoidField);

    // Iterate through features
    OGRFeature* inFeat = nullptr;
    inLayer->ResetReading();

    //sanity check
    if (inLayer->GetFeatureCount() != clusterIds.size()) {
        geomgmd::log::warning("Warning: number of features in input layer (", inLayer->GetFeatureCount(), ") does not match number of cluster ids (", clusterIds.size(), ")");
    }

    while ((inFeat = inLayer->GetNextFeature()) != nullptr) {

        // Get polygon Ids from input geopackage, and corresponding graph id
        PolygonID pid = -1;
        try {
            OGRFeatureDefn* featDef = inLayer->GetLayerDefn();
            if (featDef->GetFieldIndex("id") >= 0) {
                pid = inFeat->GetFieldAsInteger("id");
            } else {
                // fallback to OGR's built-in feature ID
                pid = static_cast<int>(inFeat->GetFID());
            }
        } catch (const std::exception& e) {
            geomgmd::log::error("Error: ", e.what());
            GDALClose(inDs);
            return;
        }
        GraphID gid = polygon_id_to_graph_id.at(pid);

        if (gid < 0 || gid >= clusterIds.size()) {
            geomgmd::log::warning("Warning: cluster ID out of range for graph id ", gid);
            OGRFeature::DestroyFeature(inFeat);
            continue;
        }

        //skip if option is set
        if (reduce_to_assigned_polys && clusterIds[gid] == -1) {
            OGRFeature::DestroyFeature(inFeat);
            continue;
        }

        int clusterValue = clusterIds[gid];
        int medoidValue = medoidInfo[gid];

        // Create output feature
        OGRFeature* outFeat = OGRFeature::CreateFeature(outLayer->GetLayerDefn());

        // Copy geometry
        outFeat->SetGeometry(inFeat->GetGeometryRef());

        // Copy existing attributes
        for (int f = 0; f < inDef->GetFieldCount(); f++) {
            outFeat->SetField(f, inFeat->GetRawFieldRef(f));
        }

        // Set new fields
        outFeat->SetField("cluster", clusterValue);
        outFeat->SetField("medoid", medoidValue);

        // Write feature
        auto err = outLayer->CreateFeature(outFeat);

        if (err != OGRERR_NONE) {
            geomgmd::log::error("Failed to create feature with id=", outFeat->GetFieldAsInteger("id"));
        }

        OGRFeature::DestroyFeature(inFeat);
        OGRFeature::DestroyFeature(outFeat);
    }

    GDALClose(inDs);
    GDALClose(outDs);

    geomgmd::log::info("Clustered polygons written to: ", outputGpkg);
}