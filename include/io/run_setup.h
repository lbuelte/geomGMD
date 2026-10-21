#ifndef GEOMGMD_IO_RUN_SETUP_H
#define GEOMGMD_IO_RUN_SETUP_H

// Paths and dataset selection shared by the executables (all given by the [IO] section of the config).

#include "io/config.h"

#include <cstddef>
#include <filesystem>

// Output folder of a run: <output_path>/<dataset>/<run_name>
std::filesystem::path run_output_path(IOConfig const &io);

// Input file with the graphs: <input_path>/<dataset>/<dataset>_graphs.json
std::filesystem::path graphs_json_path(IOConfig const &io);

// Log file of a run: global_log_file if set, otherwise <output folder>/run.log
std::filesystem::path log_file_path(IOConfig const &io, std::filesystem::path const &output_path);

// Copies the config file into the output folder as config_input.toml (replacing an existing copy).
void copy_config(std::filesystem::path const &config_path, std::filesystem::path const &output_path);

// The graphs selected by starting_instance and num_instances: positions first, ..., first + count - 1
// in the input file (num_instances is cut off at the end of the dataset).
struct InstanceRange {
    size_t first;
    size_t count;
};
InstanceRange select_instances(IOConfig const &io, size_t num_graphs);

#endif // GEOMGMD_IO_RUN_SETUP_H
