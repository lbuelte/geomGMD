#include "io/run_setup.h"

#include "utility/log.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

std::filesystem::path run_output_path(IOConfig const &io)
{
    return std::filesystem::path(io.output_path) / io.dataset / io.run_name;
}

std::filesystem::path graphs_json_path(IOConfig const &io)
{
    return std::filesystem::path(io.input_path) / io.dataset / (io.dataset + "_graphs.json");
}

std::filesystem::path log_file_path(IOConfig const &io, std::filesystem::path const &output_path)
{
    return io.global_log_file ? std::filesystem::path(io.global_log_file.value()) : output_path / "run.log";
}

void copy_config(std::filesystem::path const &config_path, std::filesystem::path const &output_path,
                 std::string const &file_name)
{
    std::filesystem::copy_file(config_path, output_path / file_name,
                               std::filesystem::copy_options::overwrite_existing);
}

void check_previous_results(std::filesystem::path const &output_path, bool overwrite)
{
    if (overwrite) return;
    std::string found;
    for (auto const *name : {"edgemaps.json", "intervaldistance_matrix.csv", "computation_info.csv",
                             "clustering.csv", "clustering.json"}) {
        if (std::filesystem::exists(output_path / name)) {
            found += (found.empty() ? "" : ", ") + std::string(name);
        }
    }
    if (not found.empty()) {
        throw std::runtime_error("The run folder " + output_path.string() + " already contains results ("
                                 + found + "). Choose another [IO] run_name, or set [IO] overwrite = true "
                                 "or use --overwrite to replace them.");
    }
}

InstanceRange select_instances(IOConfig const &io, size_t num_graphs)
{
    size_t const first = io.starting_instance;
    if (first >= num_graphs) {
        throw std::runtime_error("[IO] starting_instance = " + std::to_string(first)
                                 + " is not a position in the dataset (" + std::to_string(num_graphs) + " graphs)");
    }
    size_t const count = io.num_instances.has_value() ? std::min(io.num_instances.value(), num_graphs - first)
                                                      : num_graphs - first;
    return {first, count};
}

int run_main(int (*body)(int, char*[]), int argc, char* argv[])
{
    try {
        return body(argc, argv);
    } catch (std::exception const &e) {
        geomgmd::log::error(e.what());
        geomgmd::log::close_file();
        return EXIT_FAILURE;
    }
}
