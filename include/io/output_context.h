// Created by Laura Bülte on 2026-03-30.

#ifndef GEOMGMD_IO_OUTPUT_CONTEXT_H
#define GEOMGMD_IO_OUTPUT_CONTEXT_H

#include "result_writer.h"
#include "io/config.h"
#include "distance/geomGMDSolver.h"
#include "utility/log.h"

inline std::filesystem::path create_path(const std::filesystem::path& base_path){
    std::filesystem::create_directories(base_path);
    return base_path;
}

inline std::filesystem::path prepare_file(const std::filesystem::path& dir,
                                          const std::string& name) {
    auto file = dir / name;
    std::filesystem::remove(file); // does nothing if file doesn't exist
    return file;
}

struct OutputContext {

    const std::filesystem::path output_path;

    std::optional<JSONWriter> edgemap_writer;
    std::optional<CSVWriter> intervaldistance_matrix_writer;
    std::optional<CSVWriter> pairwise_computation_info_writer;
    // AnalysisWriters analysis;
    // VisualizationWriters viz;

    OutputContext(const std::filesystem::path& base_path, IOConfig const& config)
        : output_path(create_path(base_path))
        , edgemap_writer(
            config.write_edgemaps
                ? std::optional<JSONWriter>(prepare_file(output_path, "edgemaps.json").string())
                : std::nullopt)
        , intervaldistance_matrix_writer(
            config.write_distance_matrix
                ? std::optional<CSVWriter>(prepare_file(output_path, "intervaldistance_matrix.csv").string())
                : std::nullopt)
        , pairwise_computation_info_writer(
            config.write_distance_matrix
                ? std::optional<CSVWriter>(prepare_file(output_path, "computation_info.csv").string())
                : std::nullopt)
    {
        if (pairwise_computation_info_writer.has_value()){
            pairwise_computation_info_writer.value().write_row({"PolygonID G", "PolygonID H", "nG", "nH", "mG", "mH", "lb", "ub", "optimal", "time in ms", "status", "mip gap", "nodes"});
        }
    }
    
    void write_edgemap(const nlohmann::json& j) {
        if (not edgemap_writer.has_value()){
            geomgmd::log::error("Error: write_edgemaps is not set in config, but function write_edgemap was called. Ignoring this call.");
            return;
        }
        edgemap_writer.value().write(j);
    }

    void write_interval_distance_matrix(const std::vector<std::vector<IntervalDistance>>& matrix);

    void write_computation_info_row(const GeomGMDForiSolver &solver, IntervalDistance dist);

    std::filesystem::path make_path(const std::string& name) const {
        return output_path / name;
    }
};

#endif // GEOMGMD_IO_OUTPUT_CONTEXT_H