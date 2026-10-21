#include "io/config.h"
#include "utility/listing.h"
#include "utility/overloaded.h"

#include "toml++/toml.h"

#include <stdexcept>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace {

using geomgmd::listing::print_entry;
using geomgmd::listing::tf;

void require(bool cond, const std::string& msg)
{
    if (!cond)
        throw std::runtime_error("Config error: " + msg);
}


std::string to_lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

ClusteringConfig::ClusteringAlgorithm
parse_clustering_algo(const std::string& s_in)
{
    std::string s = to_lower(s_in);
    if (s == "clara")   return ClusteringConfig::ClusteringAlgorithm::CLARA;
    if (s == "clarans") return ClusteringConfig::ClusteringAlgorithm::CLARANS;
    if (s == "pam")     return ClusteringConfig::ClusteringAlgorithm::PAM;

    throw std::runtime_error("Unknown clustering_algo: " + s);
}

AngleType
parse_angle_type(const std::string& s_in)
{
    std::string s = to_lower(s_in);
    if (s == "signed")   return AngleType::Signed;
    if (s == "unsigned") return AngleType::Unsigned;

    throw std::runtime_error("Unknown angle_type: " + s);
}

template<typename T>
void read_required(const toml::table& t, const char* key, T& out)
{
    if (auto v = t[key].value<T>())
        out = *v;
    else
        throw std::runtime_error(
            std::string("Missing required key: ") + key);
}

template<typename T>
void read_value_if(const toml::table& t, const char* key, T& out)
{
    if (auto v = t[key].value<T>())
        out = *v;
    // else: do nothing → keep struct default
}

template<typename T>
void read_optional_if(const toml::table& t, const char* key, std::optional<T>& out)
{
    if (auto v = t[key].value<T>())
        out = *v;
    // else: remains std::nullopt
}

template<typename Enum, typename Parser>
void read_enum_if(const toml::table& t,
                  const char* key,
                  Enum& out,
                  Parser parse_fn)
{
    if (auto s = t[key].value<std::string>())
        out = parse_fn(*s);
}

LogLevel
parse_log_level_or_throw(const std::string& s)
{
    if (auto level = parse_log_level(s)) return *level;
    throw std::runtime_error("Unknown log level: " + s + " (expected error, warning, info or debug)");
}

void read_logging_section(const toml::table& tbl, LoggingConfig& logging)
{
    if (auto lg = tbl["Logging"].as_table()) {
        read_enum_if(*lg, "level", logging.level, parse_log_level_or_throw);
        read_enum_if(*lg, "file_level", logging.file_level, parse_log_level_or_throw);
        read_value_if(*lg, "gurobi_log", logging.gurobi_log);
        read_optional_if(*lg, "gurobi_log_min_gap", logging.gurobi_log_min_gap);
        if (auto node = lg->get("gurobi_log_pairs")) {
            const auto error = std::string("gurobi_log_pairs must be a list of pairs of graph ids, e.g. [[3, 19], [5, 7]]");
            auto pairs = node->as_array();
            if (not pairs) {
                throw std::runtime_error(error);
            }
            for (auto const &entry : *pairs) {
                auto pair = entry.as_array();
                if (not pair or pair->size() != 2) {
                    throw std::runtime_error(error);
                }
                auto g = (*pair)[0].value<int64_t>();
                auto h = (*pair)[1].value<int64_t>();
                if (not g or not h or *g < 0 or *h < 0) {
                    throw std::runtime_error(error);
                }
                logging.gurobi_log_pairs.emplace_back(static_cast<size_t>(*g), static_cast<size_t>(*h));
            }
        }
    }
}

// Reads the [Gurobi] section (model parameters and solver settings); missing keys keep their defaults.
void read_gurobi_section(const toml::table& tbl, ModelParameters& model, SolverSettings& solver)
{
    if (auto g = tbl["Gurobi"].as_table()) {
        read_value_if(*g, "timelimit_sec", solver.timelimit_sec);
        read_value_if(*g, "threads", solver.threads);
        read_value_if(*g, "seed", solver.seed);
        read_value_if(*g, "enforce_cyclic_order", model.enforce_cyclic_order);
        read_value_if(*g, "consider_angle_objective", model.consider_angle_objective);
        read_value_if(*g, "cost_factor_deletion_insertion", model.cost_factor_deletion_insertion);
        read_value_if(*g, "cost_factor_length_difference", model.cost_factor_length_difference);
        read_value_if(*g, "cost_factor_angle_difference", model.cost_factor_angle_difference);
        read_enum_if(*g, "angle_type", model.angle_type, parse_angle_type);
    }
}

// Reads the required [IO] section and returns the run name, if given. write_distance_matrix and
// write_edgemaps are only read if read_output_flags is set (load_dist_config does not read them).
std::optional<std::string> read_io_section(const toml::table& tbl, IOConfig& io, bool read_output_flags)
{
    auto t = tbl["IO"].as_table();
    if (not t) {
        throw std::runtime_error("Missing required [IO] section");
    }
    std::optional<std::string> run_name;
    read_required(*t, "dataset", io.dataset);
    read_required(*t, "input_path", io.input_path);
    read_required(*t, "output_path", io.output_path);
    read_optional_if(*t, "global_log_file", io.global_log_file);
    read_optional_if(*t, "run_name", run_name);
    read_value_if(*t, "generate_plots", io.generate_plots);
    if (read_output_flags) {
        read_value_if(*t, "write_distance_matrix", io.write_distance_matrix);
        read_value_if(*t, "write_edgemaps", io.write_edgemaps);
    }
    read_optional_if(*t, "num_instances", io.num_instances);
    read_value_if(*t, "starting_instance", io.starting_instance);
    require(io.num_instances.has_value() or io.starting_instance == 0,
            "Processing the whole dataset, but starting_instance is set to "
            + std::to_string(io.starting_instance));
    return run_name;
}

// Reads an array of seeds (key may be missing: the vector is kept).
void read_seeds(const toml::table& t, const char* key, std::vector<uint32_t>& seeds)
{
    if (auto arr = t.get(key)->as_array()) {
        seeds.clear();
        for (const auto& v : *arr) {
            require(v.is_integer(), "Random_seeds must be integers");
            seeds.push_back(static_cast<uint32_t>(*v.value<int64_t>()));
        }
    }
}


/* ===================== Configuration listing (for the log) ===================== */

std::string seeds_to_string(const std::vector<uint32_t>& seeds)
{
    if (seeds.empty()) return "none";
    std::ostringstream oss;
    oss << "[ ";
    for (auto s : seeds) oss << s << " ";
    oss << "]";
    return oss.str();
}

std::string format_percentage(double fraction)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (fraction * 100.0) << "%";
    return oss.str();
}

std::string sampleSizetoStr(const ClaraConfig::SampleSizeSpec& s) {
    return std::visit(overloaded{
        [&](std::monostate) {
            return std::string("setting default in clara.h (depending on k)");
        },
        [&](const ClaraConfig::FixedSample& f) {
            return std::to_string(f) + " (fixed)";
        },
        [&](const ClaraConfig::PercentageSample& p) {
            return format_percentage(p);
        }
    }, s);
}

} // end of namespace


Config load_config(const std::string& filename)
{
    toml::table tbl = toml::parse_file(filename);
    Config cfg;  // defaults are set here

    /* ===================== Gurobi ===================== */
    read_gurobi_section(tbl, cfg.model, cfg.solver);

    /* ===================== PAM ===================== */
    if (auto p = tbl["PAM"].as_table()) {
        read_value_if(*p, "num_threads_matrix", cfg.pam.num_threads_matrix);
    }

    /* ===================== Clara ===================== */
    if (auto c = tbl["Clara"].as_table()) {
        size_t sample_size = 0;
        read_value_if(*c, "sample_size", sample_size);
        double sample_fraction = 0.0;
        read_value_if(*c, "sample_fraction", sample_fraction);
        if (sample_size > 0) {
            require(sample_fraction == 0, 
                    "Both a sample size and a sample fraction were set");
            cfg.clara.sample_size = ClaraConfig::FixedSample(sample_size);
        }
        else if (sample_fraction > 0) {
            require(sample_fraction <= 1.0,
                    "Clara.sample_fraction must be in (0, 1]");
            cfg.clara.sample_size = ClaraConfig::PercentageSample(sample_fraction);
        }
        else {
            cfg.clara.sample_size = std::monostate{};
        }
        read_optional_if(*c, "min_sample_size", cfg.clara.min_sample_size);
        read_value_if(*c, "num_samples", cfg.clara.num_samples);
        read_value_if(*c, "reuse_distances", cfg.clara.reuse_distances);
        read_value_if(*c, "num_threads_assignment", cfg.clara.num_threads_assignment);
        read_seeds(*c, "random_seeds", cfg.clara.random_seeds);
        require(cfg.clara.random_seeds.size() <= cfg.clara.num_samples, 
                "Number of random_seeds exceeds num_samples");
        std::random_device rd;
        while (cfg.clara.random_seeds.size() < cfg.clara.num_samples){
            cfg.clara.random_seeds.push_back(rd());
        }
    }

    /* ===================== Clarans ===================== */
    if (auto cr = tbl["Clarans"].as_table()) {
        read_value_if(*cr, "num_local", cfg.clarans.num_local);
        read_value_if(*cr, "max_neighbors", cfg.clarans.max_neighbors);    
        read_value_if(*cr, "num_threads", cfg.clarans.num_threads);
        read_optional_if(*cr, "random_seed_local_search", 
            cfg.clarans.random_seed_local_search);
        read_seeds(*cr, "random_seeds_initialization", cfg.clarans.random_seeds_initialization);
        require(cfg.clarans.random_seeds_initialization.size() <= cfg.clarans.num_local, 
                "Number of random_seeds exceeds num_samples");
        std::random_device rd;
        while (cfg.clarans.random_seeds_initialization.size() < cfg.clarans.num_local){
            cfg.clarans.random_seeds_initialization.push_back(rd());
        }
    }

    /* ===================== IO ===================== */
    auto run_name_opt = read_io_section(tbl, cfg.io, true);

    /* ===================== Clustering ===================== */
    if (auto cl = tbl["Clustering"].as_table()) {
        read_required(*cl, "k", cfg.cluster.k);
        read_value_if(*cl, "parallel", cfg.cluster.parallel);
        read_enum_if(*cl, "clustering_algo", cfg.cluster.clustering_algo, parse_clustering_algo);
    }
    else {
        throw std::runtime_error("Missing required [Clustering] section");
    }

    /* ===================== Logging ===================== */
    read_logging_section(tbl, cfg.logging);

    // Generate default run_name from config if there was no run_name specified
    if (not run_name_opt.has_value()) {
        cfg.io.run_name = clusteringAlgoToStr(cfg.cluster.clustering_algo)
            + "_"
            + std::to_string(cfg.cluster.k);                               
    }
    else {
        cfg.io.run_name = run_name_opt.value();
    }

    return cfg;
}


DistanceConfig load_dist_config(const std::string& filename)
{
    toml::table tbl = toml::parse_file(filename);
    DistanceConfig cfg;  // defaults are set here

    /* ===================== Gurobi ===================== */
    read_gurobi_section(tbl, cfg.model, cfg.solver);

    /* ===================== IO ===================== */
    auto run_name_opt = read_io_section(tbl, cfg.io, false);

    /* ==================== Parallel ======================= */
    if (auto par = tbl["Parallel"].as_table()) {
        read_value_if(*par, "run_parallel", cfg.parallel.run_parallel);
        read_value_if(*par, "num_threads", cfg.parallel.num_threads_matrix);
        read_value_if(*par, "batch_size", cfg.parallel.default_batch_size);
    }

    /* ===================== Logging ===================== */
    read_logging_section(tbl, cfg.logging);

    // Generate default run_name from config if there was no run_name specified
    cfg.io.run_name = run_name_opt.value_or("default_run");

    return cfg;
}


std::ostream& operator<<(std::ostream& out, const IOConfig& io)
{
    out << "\n[IO]\n";
    print_entry(out, "dataset", io.dataset);
    print_entry(out, "input_path", io.input_path);
    print_entry(out, "output_path", io.output_path);
    print_entry(out, "run_name", io.run_name);
    print_entry(out, "global_log_file",
                io.global_log_file ? *io.global_log_file : "log file will be generated in run output");
    print_entry(out, "generate_plots", tf(io.generate_plots));
    print_entry(out, "write_distance_matrix", tf(io.write_distance_matrix));
    print_entry(out, "write_edgemaps", tf(io.write_edgemaps));
    print_entry(out, "num_instances", io.num_instances ? std::to_string(*io.num_instances) : "all");
    print_entry(out, "starting_instance", io.starting_instance);
    return out;
}

std::ostream& operator<<(std::ostream& out, const PAMConfig& pam)
{
    out << "\n[PAM]\n";
    print_entry(out, "num_threads_matrix", pam.num_threads_matrix);
    return out;
}

std::ostream& operator<<(std::ostream& out, const ClaraConfig& clara)
{
    out << "\n[Clara]\n";
    print_entry(out, "sample_size", sampleSizetoStr(clara.sample_size));
    if (clara.min_sample_size.has_value()) {
        print_entry(out, "min_sample_size", clara.min_sample_size.value());
    }
    print_entry(out, "num_samples", clara.num_samples);
    print_entry(out, "reuse_distances", tf(clara.reuse_distances));
    print_entry(out, "num_threads_assignment", clara.num_threads_assignment);
    print_entry(out, "random_seeds", seeds_to_string(clara.random_seeds));
    return out;
}

std::ostream& operator<<(std::ostream& out, const ClaransConfig& clarans)
{
    out << "\n[Clarans]\n";
    print_entry(out, "num_local", clarans.num_local);
    print_entry(out, "max_neighbors", clarans.max_neighbors);
    print_entry(out, "num_threads", clarans.num_threads);
    print_entry(out, "random_seeds_initialization", seeds_to_string(clarans.random_seeds_initialization));
    print_entry(out, "random_seed_local_search",
                clarans.random_seed_local_search ? std::to_string(*clarans.random_seed_local_search)
                                                 : "nondeterministic");
    return out;
}

std::ostream& operator<<(std::ostream& out, const ClusteringConfig& cluster)
{
    out << "\n[Clustering]\n";
    print_entry(out, "k", cluster.k);
    print_entry(out, "clustering_algo", clusteringAlgoToStr(cluster.clustering_algo));
    print_entry(out, "parallel", tf(cluster.parallel));
    return out;
}

std::ostream& operator<<(std::ostream& out, const ParallelConfig& parallel)
{
    out << "\n[Parallel]\n";
    print_entry(out, "run_parallel", tf(parallel.run_parallel));
    print_entry(out, "num_threads", parallel.num_threads_matrix);
    print_entry(out, "batch_size", parallel.default_batch_size);
    return out;
}

std::ostream& operator<<(std::ostream& out, const LoggingConfig& logging)
{
    out << "\n[Logging]\n";
    print_entry(out, "level", to_string(logging.level));
    print_entry(out, "file_level", to_string(logging.file_level));
    print_entry(out, "gurobi_log", tf(logging.gurobi_log));
    if (logging.gurobi_log) {
        std::ostringstream pairs;
        pairs << "[";
        for (size_t i = 0; i < logging.gurobi_log_pairs.size(); ++i) {
            pairs << (i > 0 ? ", " : "") << "[" << logging.gurobi_log_pairs[i].first << ", "
                  << logging.gurobi_log_pairs[i].second << "]";
        }
        pairs << "]";
        print_entry(out, "gurobi_log_pairs", pairs.str());
        print_entry(out, "gurobi_log_min_gap",
                    logging.gurobi_log_min_gap ? std::to_string(*logging.gurobi_log_min_gap) : "not set");
    }
    return out;
}

std::ostream& operator<<(std::ostream& out, const Config& cfg)
{
    out << "================ Configuration ================\n";
    out << "\n[Gurobi]\n" << cfg.solver << cfg.model;
    out << cfg.pam << cfg.clara << cfg.clarans << cfg.io << cfg.cluster << cfg.logging;
    out << "\n================================================\n\n";
    return out;
}

std::ostream& operator<<(std::ostream& out, const DistanceConfig& cfg)
{
    out << "================ Configuration ================\n";
    out << "\n[Gurobi]\n" << cfg.solver << cfg.model;
    out << cfg.io << cfg.parallel << cfg.logging;
    out << "\n================================================\n\n";
    return out;
}
