#ifndef GEOMGMD_IO_OUTPUT_CONTEXT_H
#define GEOMGMD_IO_OUTPUT_CONTEXT_H

#include "result_writer.h"
#include "io/config.h"
#include "distance/geomGMD.h"
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

// Output files of a run. Files that are disabled in the config ([IO] write_edgemaps,
// write_distance_matrix) are not created, and the corresponding write functions do nothing.
struct OutputContext {

    const std::filesystem::path output_path;

    std::optional<JSONWriter> edgemap_writer;
    std::optional<CSVWriter> intervaldistance_matrix_writer;
    std::optional<CSVWriter> pairwise_computation_info_writer;

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
            pairwise_computation_info_writer.value().write_row({"G", "H", "nG", "nH", "mG", "mH", "lb", "ub", "optimal", "time in ms", "status", "mip gap", "nodes"});
        }
    }
    
    // One line of edgemaps.json (if write_edgemaps)
    void write_edgemap(const nlohmann::json& j) {
        if (edgemap_writer.has_value()){
            edgemap_writer.value().write(j);
        }
    }

    // intervaldistance_matrix.csv (if write_distance_matrix). matrix[i][j] is the distance between the graphs
    // with ids first_graph_id + i and first_graph_id + j; row and col in the file are these graph ids.
    // Pairs without a distance (placeholder {-1, -1}: failed or not computed) are written as nan,nan,false.
    void write_interval_distance_matrix(const std::vector<std::vector<IntervalDistance>>& matrix,
                                        GraphID first_graph_id);

    // One row of computation_info.csv for a computed pair (if write_distance_matrix; nothing is written
    // if no distance was computed).
    void write_computation_info_row(const GeomGMDResult &result);
};

#endif // GEOMGMD_IO_OUTPUT_CONTEXT_H