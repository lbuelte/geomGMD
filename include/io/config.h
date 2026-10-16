#ifndef GEOMGMD_CONFIG_H
#define GEOMGMD_CONFIG_H

#include <string>
#include <ostream>
#include <optional>
#include <vector>
#include <cstdint>
#include <variant>

#include "utility/geometry_base.h"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;


struct GurobiConfig {

    int timelimit_sec = 60;

    bool enforce_cyclic_order = false;
    bool consider_angle_objective = true;

    AngleType angle_type = AngleType::Signed;
    
    double cost_factor_deletion_insertion = 1.0;
    double cost_factor_length_difference = 1.0;
    double cost_factor_angle_difference = 2.0;
};

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
    bool write_distance_matrix = true;
    bool write_nodemaps = true;

    std::optional<size_t> num_instances;
    size_t starting_instance = 0;
};

struct ClusteringConfig {

    enum class Mode {
        SingleRun,
        DiminishingVA
    };
    Mode mode = Mode::SingleRun;

    size_t k;

    enum class ClusteringAlgorithm {
        CLARA, CLARANS, PAM
    };
    ClusteringAlgorithm clustering_algo = ClusteringAlgorithm::CLARA;

    bool parallel = true;
    bool write_dist_matrix = true;

    struct VAConfig {
        double starting_va;             
        double va_diff;        
        size_t max_iterations;

        bool fixed_k = true;
    };
    std::optional<VAConfig> va_mode;

    double single_run_va = 0.0;
};

struct Config {
    GurobiConfig gurobi;
    PAMConfig pam;
    ClaraConfig clara;
    ClaransConfig clarans;
    IOConfig io;
    ClusteringConfig cluster;
};

Config load_config(const std::string& filename);
void log_config(const Config& cfg, std::ostream& out);

struct ParallelConfig {
    bool run_parallel = true;
    size_t num_threads_matrix = 4;
    size_t default_batch_size = 10;
};

struct DistanceConfig {
    GurobiConfig gurobi;
    IOConfig io;
    ParallelConfig parallel;
};

DistanceConfig load_dist_config(const std::string& filename);
void log_config(const DistanceConfig& cfg, std::ostream& out);


// Helper to print ClusteringAlgorithm enum
inline std::string clusteringAlgoToStr(ClusteringConfig::ClusteringAlgorithm algo) {
    switch (algo) {
        case ClusteringConfig::ClusteringAlgorithm::CLARA:   return "CLARA";
        case ClusteringConfig::ClusteringAlgorithm::CLARANS: return "CLARANS";
        case ClusteringConfig::ClusteringAlgorithm::PAM:     return "PAM";
    }
    return "UNKNOWN";
}

#endif // GEOMGMD_CONFIG_H