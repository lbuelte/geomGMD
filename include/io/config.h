#ifndef GEOMGMD_IO_CONFIG_H
#define GEOMGMD_IO_CONFIG_H

#include <string>
#include <ostream>
#include <optional>
#include <vector>
#include <cstdint>
#include <variant>
#include <utility>

#include "utility/geometry_base.h"
#include "distance/parameters.h"
#include "utility/log.h"

struct PAMConfig {
    size_t num_threads_matrix = 8;
};

struct ClaraConfig {
    using FixedSample = size_t;
    using PercentageSample = double;
    using SampleSizeSpec = std::variant<std::monostate, FixedSample, PercentageSample>;

    SampleSizeSpec sample_size;
    std::optional<size_t> min_sample_size;
    size_t num_samples = 1;
    size_t num_threads_assignment = 8;

    bool reuse_distances = true;

    // TODO: For the diminishing va mode, we might want more seeds than num_samples,
    // since we run CLARA multiple times. 
    std::vector<uint32_t> random_seeds;
};

struct ClaransConfig {
    size_t num_local = 1;
    size_t max_neighbors = 100;

    unsigned num_threads = 8;
    std::vector<uint32_t> random_seeds_initialization;
    std::optional<uint32_t> random_seed_local_search;
};

struct IOConfig {
    std::string dataset;
    std::string input_path;
    std::string output_path;
    std::optional<std::string> global_log_file;
    std::string run_name;
    bool generate_plots = true;
    bool write_distance_matrix = true;  // intervaldistance_matrix.csv and computation_info.csv
    bool write_edgemaps = true;         // edgemaps.json
    bool overwrite = false;             // allow replacing the results of a previous run in the run folder

    std::optional<size_t> num_instances;
    size_t starting_instance = 0;
};

struct ClusteringConfig {

    size_t k;

    enum class ClusteringAlgorithm {
        CLARA, CLARANS, PAM
    };
    ClusteringAlgorithm clustering_algo = ClusteringAlgorithm::CLARA;

    bool parallel = true;
};

// Log levels of console and log file (see utility/log.h); can be overridden on the command line.
struct LoggingConfig {
    LogLevel level = LogLevel::Info;        // console
    LogLevel file_level = LogLevel::Debug;  // log file

    // Gurobi's own log per computed pair (written at level debug, prefixed with the pair "[g,h]").
    // If enabled without filters, the log of every pair is written. Otherwise only pairs in
    // gurobi_log_pairs (graph ids = positions in the input file, order irrelevant) and/or pairs whose
    // final gap exceeds gurobi_log_min_gap. Pairs with a solver error are always written.
    bool gurobi_log = false;
    std::vector<std::pair<size_t, size_t>> gurobi_log_pairs;
    std::optional<double> gurobi_log_min_gap;
};

// Listings of the sections for the log: section header and one "  key = value" line per key
// (keys as in the TOML file)
std::ostream& operator<<(std::ostream& out, const PAMConfig& pam);
std::ostream& operator<<(std::ostream& out, const ClaraConfig& clara);
std::ostream& operator<<(std::ostream& out, const ClaransConfig& clarans);
std::ostream& operator<<(std::ostream& out, const IOConfig& io);
std::ostream& operator<<(std::ostream& out, const ClusteringConfig& cluster);
std::ostream& operator<<(std::ostream& out, const LoggingConfig& logging);

struct Config {
    ModelParameters model;     // [Gurobi] section in the TOML file
    SolverSettings solver;     // [Gurobi] section in the TOML file
    PAMConfig pam;
    ClaraConfig clara;
    ClaransConfig clarans;
    IOConfig io;
    ClusteringConfig cluster;
    LoggingConfig logging;

    // Warnings while reading the file (e.g. ignored sections), to be logged once logging is set up
    std::vector<std::string> warnings;
};

Config load_config(const std::string& filename);
// Listing of the whole configuration in TOML layout (for the log)
std::ostream& operator<<(std::ostream& out, const Config& cfg);

struct ParallelConfig {
    size_t num_threads_matrix = 4;
    size_t default_batch_size = 10;
};

std::ostream& operator<<(std::ostream& out, const ParallelConfig& parallel);

struct DistanceConfig {
    ModelParameters model;     // [Gurobi] section in the TOML file
    SolverSettings solver;     // [Gurobi] section in the TOML file
    IOConfig io;
    ParallelConfig parallel;
    LoggingConfig logging;

    // Warnings while reading the file (e.g. ignored sections), to be logged once logging is set up
    std::vector<std::string> warnings;
};

DistanceConfig load_dist_config(const std::string& filename);
// Listing of the whole configuration in TOML layout (for the log)
std::ostream& operator<<(std::ostream& out, const DistanceConfig& cfg);


// Helper to print ClusteringAlgorithm enum
inline std::string clusteringAlgoToStr(ClusteringConfig::ClusteringAlgorithm algo) {
    switch (algo) {
        case ClusteringConfig::ClusteringAlgorithm::CLARA:   return "CLARA";
        case ClusteringConfig::ClusteringAlgorithm::CLARANS: return "CLARANS";
        case ClusteringConfig::ClusteringAlgorithm::PAM:     return "PAM";
    }
    return "UNKNOWN";
}

#endif // GEOMGMD_IO_CONFIG_H