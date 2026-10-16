#include "io/config.h"
#include "io/logger.h"

#include "toml++/toml.h"

#include <stdexcept>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace {

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

// Helper to print booleans as true/false
const char* tf(bool b) { return b ? "true" : "false"; }

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

    std::optional<std::string> run_name_opt = std::nullopt;

    /* ===================== Gurobi ===================== */
    if (auto g = tbl["Gurobi"].as_table()) {
        read_value_if(*g, "timelimit_sec", cfg.gurobi.timelimit_sec);
        read_value_if(*g, "enforce_cyclic_order", 
                      cfg.gurobi.enforce_cyclic_order);
        read_value_if(*g, "consider_angle_objective", 
                      cfg.gurobi.consider_angle_objective);
        read_value_if(*g, "cost_factor_deletion_insertion",
                      cfg.gurobi.cost_factor_deletion_insertion);
        read_value_if(*g, "cost_factor_length_difference",
                      cfg.gurobi.cost_factor_length_difference);
        read_value_if(*g, "cost_factor_angle_difference",
                      cfg.gurobi.cost_factor_angle_difference);
        read_enum_if(*g, "angle_type", cfg.gurobi.angle_type, parse_angle_type);
    }

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
        if (auto arr = c->get("random_seeds")->as_array()) {
            cfg.clara.random_seeds.clear();
            for (const auto& v : *arr) {
                require(v.is_integer(), "Random_seeds must be integers");
                cfg.clara.random_seeds.push_back(
                    static_cast<uint32_t>(*v.value<int64_t>()));
            }
        }
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
        if (auto arr = cr->get("random_seeds_initialization")->as_array()) {
            cfg.clarans.random_seeds_initialization.clear();
            for (const auto& v : *arr) {
                require(v.is_integer(), "Random_seeds must be integers");
                cfg.clarans.random_seeds_initialization.push_back(
                    static_cast<uint32_t>(*v.value<int64_t>()));
            }
        }
        require(cfg.clarans.random_seeds_initialization.size() <= cfg.clarans.num_local, 
                "Number of random_seeds exceeds num_samples");
        std::random_device rd;
        while (cfg.clarans.random_seeds_initialization.size() < cfg.clarans.num_local){
            cfg.clarans.random_seeds_initialization.push_back(rd());
        }
    }

    /* ===================== IO ===================== */
    if (auto io = tbl["IO"].as_table()) {
        read_required(*io, "dataset", cfg.io.dataset);
        read_required(*io, "input_path", cfg.io.input_path);
        read_required(*io, "output_path", cfg.io.output_path);
        read_optional_if(*io, "global_log_file", cfg.io.global_log_file);
        read_optional_if(*io, "run_name", run_name_opt);
        read_value_if(*io, "generate_plots", cfg.io.generate_plots);
        read_value_if(*io, "write_distance_matrix", cfg.io.write_distance_matrix);
        read_value_if(*io, "write_edgemaps", cfg.io.write_edgemaps);
        read_optional_if(*io, "num_instances", cfg.io.num_instances);
        read_value_if(*io, "starting_instance", cfg.io.starting_instance);
        require(cfg.io.num_instances.has_value() or cfg.io.starting_instance == 0,
                "Processing the whole dataset, but starting_instance is set to " 
                + cfg.io.starting_instance);
    }
    else {
        throw std::runtime_error("Missing required [IO] section");
    }

    /* ===================== Clustering ===================== */
    if (auto cl = tbl["Clustering"].as_table()) {
        read_required(*cl, "k", cfg.cluster.k);
        read_value_if(*cl, "parallel", cfg.cluster.parallel);
        read_enum_if(*cl, "clustering_algo", cfg.cluster.clustering_algo, parse_clustering_algo);
    }
    else {
        throw std::runtime_error("Missing required [Clustering] section");
    }

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


// Main logging function
void log_config(const Config& cfg, std::ostream& out)
{
    out << "================ Configuration ================\n";

    /* ===================== IO ===================== */
    out << "\n[IO]\n";
    out << "  dataset                 = " << cfg.io.dataset << '\n';
    out << "  input_path              = " << cfg.io.input_path << '\n';
    out << "  output_path             = " << cfg.io.output_path << '\n';
    out << "  run_name                = " << cfg.io.run_name << '\n';
    out << "  global_log_file         = " 
        << (cfg.io.global_log_file ? *cfg.io.global_log_file
                                   : "log file will be generated in run output") << '\n';
    out << "  plots generated         = " << tf(cfg.io.generate_plots) << '\n';
    out << "  writing distance matrix = " << tf(cfg.io.write_distance_matrix) << '\n';
    out << "  writing edgemaps = " << tf(cfg.io.write_edgemaps) << '\n';
    out << "  num_instances           = "
        << (cfg.io.num_instances ? std::to_string(*cfg.io.num_instances)
                                 : "all") << '\n';
    out << "  starting_instance       = " << cfg.io.starting_instance << '\n';

    /* ===================== Clustering ===================== */
    out << "\n[Clustering]\n";
    out << "  k                  = " << cfg.cluster.k << '\n';
    out << "  clustering_algo    = "
        << clusteringAlgoToStr(cfg.cluster.clustering_algo) << '\n';
    out << "  parallel           = " << tf(cfg.cluster.parallel) << '\n';

    /* ===================== Clara ===================== */
    out << "\n[Clara]\n";
    out << "  sample_size            = " 
        << sampleSizetoStr(cfg.clara.sample_size) << '\n';
    if (cfg.clara.min_sample_size.has_value())
    out << "  min_sample_size        = " << cfg.clara.min_sample_size.value() << '\n';
    out << "  num_samples            = " << cfg.clara.num_samples << '\n';
    out << "  reuse_distances        = " << tf(cfg.clara.reuse_distances) << '\n';
    out << "  num_threads_assignment = " << cfg.clara.num_threads_assignment << '\n';    
    out << "  random_seeds           = ";
    if (cfg.clara.random_seeds.empty()) {
        out << "none";
    } else {
        out << "[ ";
        for (auto s : cfg.clara.random_seeds)
            out << s << " ";
        out << "]";
    }
    out << '\n';

    /* ===================== Clarans ===================== */
    out << "\n[Clarans]\n";
    out << "  num_local                  = "
        << cfg.clarans.num_local << '\n';
    out << "  max_neighbors              = "
        << cfg.clarans.max_neighbors << '\n';
    out << "  num_threads                = "
        << cfg.clarans.num_threads << '\n';
    out << "  random_seed_initialization = ";
    if (cfg.clarans.random_seeds_initialization.empty()) {
        out << "none";
    } else {
        out << "[ ";
        for (auto s : cfg.clarans.random_seeds_initialization)
            out << s << " ";
        out << "]";
    }
    out << '\n';
    out << "  random_seed_local_search   = "
        << (cfg.clarans.random_seed_local_search
                ? std::to_string(*cfg.clarans.random_seed_local_search)
                : "nondeterministic") << '\n';


    /* ===================== PAM ===================== */   
    out << "\n[PAM]\n";
    out << "  num_threads_matrix = " << cfg.pam.num_threads_matrix << '\n';                       

    /* ===================== Gurobi ===================== */
    out << "\n[Gurobi]\n";
    out << "  timelimit_sec                  = "
        << cfg.gurobi.timelimit_sec << '\n';
    out << "  enforce_cyclic_order           = "
        << cfg.gurobi.enforce_cyclic_order << '\n';
    out << "  consider_angle_objective       = "
        << cfg.gurobi.consider_angle_objective << '\n';
    out << "  cost_factor_deletion_insertion = "
        << cfg.gurobi.cost_factor_deletion_insertion << '\n';
    out << "  cost_factor_length_difference  = "
        << cfg.gurobi.cost_factor_length_difference << '\n';
    out << "  cost_factor_angle_difference   = "
        << cfg.gurobi.cost_factor_angle_difference << '\n';
    out << "  angle_type                     = "
        << cfg.gurobi.angle_type << '\n';

    out << "\n================================================\n\n";
}


DistanceConfig load_dist_config(const std::string& filename)
{
    toml::table tbl = toml::parse_file(filename);
    DistanceConfig cfg;  // defaults are set here

    std::optional<std::string> run_name_opt = std::nullopt;

    /* ===================== Gurobi ===================== */
    if (auto g = tbl["Gurobi"].as_table()) {
        read_value_if(*g, "timelimit_sec", cfg.gurobi.timelimit_sec);
        read_value_if(*g, "enforce_cyclic_order", 
                      cfg.gurobi.enforce_cyclic_order);
        read_value_if(*g, "consider_angle_objective", 
                      cfg.gurobi.consider_angle_objective);
        read_value_if(*g, "cost_factor_deletion_insertion",
                      cfg.gurobi.cost_factor_deletion_insertion);
        read_value_if(*g, "cost_factor_length_difference",
                      cfg.gurobi.cost_factor_length_difference);
        read_value_if(*g, "cost_factor_angle_difference",
                      cfg.gurobi.cost_factor_angle_difference);
        read_enum_if(*g, "angle_type", cfg.gurobi.angle_type, parse_angle_type);
    }

    /* ===================== IO ===================== */
    if (auto io = tbl["IO"].as_table()) {
        read_required(*io, "dataset", cfg.io.dataset);
        read_required(*io, "input_path", cfg.io.input_path);
        read_required(*io, "output_path", cfg.io.output_path);
        read_optional_if(*io, "global_log_file", cfg.io.global_log_file);
        read_optional_if(*io, "run_name", run_name_opt);
        read_value_if(*io, "generate_plots", cfg.io.generate_plots);
        read_optional_if(*io, "num_instances", cfg.io.num_instances);
        read_value_if(*io, "starting_instance", cfg.io.starting_instance);
        require(cfg.io.num_instances.has_value() or cfg.io.starting_instance == 0,
                "Processing the whole dataset, but starting_instance is set to " 
                + cfg.io.starting_instance);
    }
    else {
        throw std::runtime_error("Missing required [IO] section");
    }

    /* ==================== Parallel ======================= */
    if (auto par = tbl["Parallel"].as_table()) {
        read_value_if(*par, "run_parallel", cfg.parallel.run_parallel);
        read_value_if(*par, "num_threads", cfg.parallel.num_threads_matrix);
        read_value_if(*par, "batch_size", cfg.parallel.default_batch_size);
    }

    // Generate default run_name from config if there was no run_name specified
    if (not run_name_opt.has_value()) {
        cfg.io.run_name = "default_run";                          
    }
    else {
        cfg.io.run_name = run_name_opt.value();
    }

    return cfg;
}


void log_config(const DistanceConfig& cfg, std::ostream& out)
{
    out << "================ Configuration ================\n";

    /* ===================== IO ===================== */
    out << "\n[IO]\n";
    out << "  dataset           = " << cfg.io.dataset << '\n';
    out << "  input_path        = " << cfg.io.input_path << '\n';
    out << "  output_path       = " << cfg.io.output_path << '\n';
    out << "  run_name          = " << cfg.io.run_name << '\n';
    out << "  global_log_file   = " 
        << (cfg.io.global_log_file ? *cfg.io.global_log_file
                                   : "log file will be generated in run output") << '\n';
    out << "  plots generated   = " << tf(cfg.io.generate_plots) << '\n';
    out << "  num_instances     = "
        << (cfg.io.num_instances ? std::to_string(*cfg.io.num_instances)
                                 : "all") << '\n';
    out << "  starting_instance = " << cfg.io.starting_instance << '\n';

    /* ===================== Gurobi ===================== */
    out << "\n[Gurobi]\n";
    out << "  timelimit_sec                  = "
        << cfg.gurobi.timelimit_sec << '\n';
    out << "  enforce_cyclic_order           = "
        << cfg.gurobi.enforce_cyclic_order << '\n';
    out << "  consider_angle_objective       = "
        << cfg.gurobi.consider_angle_objective << '\n';
    out << "  cost_factor_deletion_insertion = "
        << cfg.gurobi.cost_factor_deletion_insertion << '\n';
    out << "  cost_factor_length_difference  = "
        << cfg.gurobi.cost_factor_length_difference << '\n';
    out << "  cost_factor_angle_difference   = "
        << cfg.gurobi.cost_factor_angle_difference << '\n';
    out << "  angle_type                     = "
        << cfg.gurobi.angle_type << '\n';

    /* ===================== Parallel ===================== */
    out << "\n[Parallel]\n";
    out << "  run_parallel       = " << tf(cfg.parallel.run_parallel) << '\n';
    out << "  num_threads_matrix = " << cfg.parallel.num_threads_matrix << '\n';
    out << "  default_batch_size = " << cfg.parallel.default_batch_size << '\n';

    out << "\n================================================\n\n";
}