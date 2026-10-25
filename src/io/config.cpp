#include "io/config.h"
#include "utility/listing.h"
#include "utility/overloaded.h"

#include "toml++/toml.h"

#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <random>
#include <sstream>
#include <iomanip>
#include <string_view>
#include <type_traits>

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

ClusteringConfig::FailedDistances
parse_failed_distances(const std::string& s_in)
{
    std::string s = to_lower(s_in);
    if (s == "abort")    return ClusteringConfig::FailedDistances::Abort;
    if (s == "infinite") return ClusteringConfig::FailedDistances::Infinite;
    if (s == "exclude")  return ClusteringConfig::FailedDistances::Exclude;

    throw std::runtime_error("Unknown failed_distances: " + s + " (expected abort, infinite or exclude)");
}

LogLevel
parse_log_level_or_throw(const std::string& s)
{
    if (auto level = parse_log_level(s)) return *level;
    throw std::runtime_error("Unknown log level: " + s + " (expected error, warning, info or debug)");
}


// Description of the values accepted for a key of type T (for error messages)
template<typename T>
std::string expected_value()
{
    if constexpr (std::is_same_v<T, bool>)                             return "true or false";
    else if constexpr (std::is_same_v<T, std::string>)                 return "a string";
    else if constexpr (std::is_integral_v<T> and std::is_unsigned_v<T>) return "a non-negative integer";
    else if constexpr (std::is_integral_v<T>)                          return "an integer";
    else                                                               return "a number";
}

// A section [name] of the config file. Reading a key checks its type: booleans and strings must be
// given as such; for numbers, integers and floating-point values are interchangeable as long as the
// value is represented exactly (e.g. timelimit_sec = 300.0 is fine, 300.5 is not).
class Section {
public:
    Section(const toml::table& table, std::string name) : _table(table), _name(std::move(name)) {}

    const std::string& name() const { return _name; }
    std::string key_name(std::string_view key) const { return "[" + _name + "] " + std::string(key); }

    const toml::node* get(std::string_view key) const { return _table.get(key); }

    // Throws for keys that are not in valid_keys (e.g. typos, which would otherwise be ignored silently)
    void check_keys(std::initializer_list<std::string_view> valid_keys) const
    {
        for (auto const& [key, node] : _table) {
            if (std::find(valid_keys.begin(), valid_keys.end(), key.str()) != valid_keys.end()) continue;
            std::string valid;
            for (auto k : valid_keys) valid += (valid.empty() ? "" : ", ") + std::string(k);
            throw std::runtime_error("Config error: unknown key " + key_name(key.str())
                                     + " (valid keys: " + valid + ")");
        }
    }

    // Missing key: out keeps its value (the default)
    template<typename T>
    void read(std::string_view key, T& out) const
    {
        if (auto node = get(key)) out = value<T>(*node, key);
    }

    // Missing key: out remains std::nullopt
    template<typename T>
    void read(std::string_view key, std::optional<T>& out) const
    {
        if (auto node = get(key)) out = value<T>(*node, key);
    }

    template<typename T>
    void read_required(std::string_view key, T& out) const
    {
        auto node = get(key);
        if (not node) throw std::runtime_error("Config error: missing required key " + key_name(key));
        out = value<T>(*node, key);
    }

    template<typename Enum, typename Parser>
    void read_enum(std::string_view key, Enum& out, Parser parse_fn) const
    {
        if (auto node = get(key)) out = parse_fn(value<std::string>(*node, key));
    }

private:
    template<typename T>
    T value(const toml::node& node, std::string_view key) const
    {
        std::optional<T> v;
        if constexpr (std::is_same_v<T, bool>) {
            if (node.is_boolean()) v = node.value<bool>();
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (node.is_string()) v = node.value<std::string>();
        } else {
            if (node.is_number()) v = node.value<T>();
        }
        if (not v) throw std::runtime_error("Config error: " + key_name(key) + " must be " + expected_value<T>());
        return *v;
    }

    const toml::table& _table;
    std::string _name;
};

// The section [name] of the file, if present (error if name is not a table)
std::optional<Section> get_section(const toml::table& tbl, const std::string& name)
{
    auto node = tbl.get(name);
    if (not node) return std::nullopt;
    if (not node->is_table()) throw std::runtime_error("Config error: " + name + " must be a section [" + name + "]");
    return Section(*node->as_table(), name);
}

// Sections that any geomGMD program reads; a program ignores the ones it does not need (with a warning).
const std::vector<std::string> all_sections = {"Gurobi", "PAM", "Clara", "Clarans", "IO", "Clustering", "Parallel", "Logging"};

// Throws for unknown sections (e.g. typos) and returns warnings for known sections that this program ignores.
std::vector<std::string> check_sections(const toml::table& tbl, std::initializer_list<std::string_view> used)
{
    std::vector<std::string> warnings;
    for (auto const& [key, node] : tbl) {
        const std::string name(key.str());
        if (std::find(used.begin(), used.end(), name) != used.end()) continue;
        if (std::find(all_sections.begin(), all_sections.end(), name) == all_sections.end()) {
            std::string valid;
            for (auto k : used) valid += (valid.empty() ? "" : ", ") + std::string(k);
            throw std::runtime_error("Config error: unknown section or key " + name
                                     + " (sections of this program: " + valid + ")");
        }
        warnings.push_back("Config: section [" + name + "] is not used by this program and is ignored");
    }
    return warnings;
}

// Parses the config file; syntax errors and unreadable files give an error with file name and position.
toml::table parse_config_file(const std::string& filename)
{
    try {
        return toml::parse_file(filename);
    } catch (const toml::parse_error& e) {
        std::ostringstream msg;
        msg << "Config error: could not read " << filename << ": " << e.description();
        if (e.source().begin) {
            msg << " (line " << e.source().begin.line << ", column " << e.source().begin.column << ")";
        }
        throw std::runtime_error(msg.str());
    }
}

void require_positive(const Section& s, std::string_view key, double value)
{
    require(value > 0, s.key_name(key) + " must be > 0");
}

void require_non_negative(const Section& s, std::string_view key, double value)
{
    require(value >= 0 and std::isfinite(value), s.key_name(key) + " must be >= 0");
}


void read_logging_section(const toml::table& tbl, LoggingConfig& logging)
{
    if (auto lg = get_section(tbl, "Logging")) {
        lg->check_keys({"level", "file_level", "gurobi_log", "gurobi_log_pairs", "gurobi_log_min_gap"});
        lg->read_enum("level", logging.level, parse_log_level_or_throw);
        lg->read_enum("file_level", logging.file_level, parse_log_level_or_throw);
        lg->read("gurobi_log", logging.gurobi_log);
        lg->read("gurobi_log_min_gap", logging.gurobi_log_min_gap);
        if (logging.gurobi_log_min_gap) require_non_negative(*lg, "gurobi_log_min_gap", *logging.gurobi_log_min_gap);
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
    if (auto g = get_section(tbl, "Gurobi")) {
        g->check_keys({"timelimit_sec", "threads", "seed", "mip_gap", "enforce_cyclic_order", "consider_angle_objective",
                       "cost_factor_deletion_insertion", "cost_factor_length_difference",
                       "cost_factor_angle_difference", "angle_type"});
        g->read("timelimit_sec", solver.timelimit_sec);
        g->read("threads", solver.threads);
        g->read("seed", solver.seed);
        g->read("mip_gap", solver.mip_gap);
        g->read("enforce_cyclic_order", model.enforce_cyclic_order);
        g->read("consider_angle_objective", model.consider_angle_objective);
        g->read("cost_factor_deletion_insertion", model.cost_factor_deletion_insertion);
        g->read("cost_factor_length_difference", model.cost_factor_length_difference);
        g->read("cost_factor_angle_difference", model.cost_factor_angle_difference);
        g->read_enum("angle_type", model.angle_type, parse_angle_type);

        require_positive(*g, "timelimit_sec", solver.timelimit_sec);
        require(solver.threads >= 0, g->key_name("threads") + " must be >= 0 (0: chosen by Gurobi)");
        require(solver.seed >= 0, g->key_name("seed") + " must be >= 0");
        require_non_negative(*g, "mip_gap", solver.mip_gap);
        require_non_negative(*g, "cost_factor_deletion_insertion", model.cost_factor_deletion_insertion);
        require_non_negative(*g, "cost_factor_length_difference", model.cost_factor_length_difference);
        require_non_negative(*g, "cost_factor_angle_difference", model.cost_factor_angle_difference);
    }
}

// Reads the required [IO] section and returns the run name, if given.
std::optional<std::string> read_io_section(const toml::table& tbl, IOConfig& io)
{
    auto t = get_section(tbl, "IO");
    if (not t) {
        throw std::runtime_error("Missing required [IO] section");
    }
    t->check_keys({"dataset", "input_path", "output_path", "global_log_file", "run_name", "generate_plots",
                   "write_distance_matrix", "write_edgemaps", "overwrite", "num_instances", "starting_instance"});
    std::optional<std::string> run_name;
    t->read_required("dataset", io.dataset);
    t->read_required("input_path", io.input_path);
    t->read_required("output_path", io.output_path);
    t->read("global_log_file", io.global_log_file);
    t->read("run_name", run_name);
    t->read("generate_plots", io.generate_plots);
    t->read("write_distance_matrix", io.write_distance_matrix);
    t->read("write_edgemaps", io.write_edgemaps);
    t->read("overwrite", io.overwrite);
    t->read("num_instances", io.num_instances);
    t->read("starting_instance", io.starting_instance);
    require(not io.num_instances.has_value() or *io.num_instances > 0, t->key_name("num_instances") + " must be > 0");
    require(io.num_instances.has_value() or io.starting_instance == 0,
            "Processing the whole dataset, but starting_instance is set to "
            + std::to_string(io.starting_instance));
    return run_name;
}

// Reads an array of seeds (key may be missing: the vector is kept).
void read_seeds(const Section& s, std::string_view key, std::vector<uint32_t>& seeds)
{
    auto node = s.get(key);
    if (not node) return;
    auto arr = node->as_array();
    require(arr != nullptr, s.key_name(key) + " must be a list of integers, e.g. [1, 2, 3]");
    seeds.clear();
    for (const auto& v : *arr) {
        auto const seed = v.is_integer() ? v.value<int64_t>() : std::nullopt;
        if (not seed or *seed < 0 or *seed > std::numeric_limits<uint32_t>::max()) {
            throw std::runtime_error("Config error: " + s.key_name(key) + " must be a list of integers between 0 and "
                                     + std::to_string(std::numeric_limits<uint32_t>::max()));
        }
        seeds.push_back(static_cast<uint32_t>(*seed));
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
    toml::table tbl = parse_config_file(filename);
    Config cfg;  // defaults are set here
    cfg.warnings = check_sections(tbl, {"Gurobi", "PAM", "Clara", "Clarans", "IO", "Clustering", "Logging"});

    /* ===================== Gurobi ===================== */
    read_gurobi_section(tbl, cfg.model, cfg.solver);

    /* ===================== PAM ===================== */
    if (auto p = get_section(tbl, "PAM")) {
        p->check_keys({"num_threads_matrix"});
        p->read("num_threads_matrix", cfg.pam.num_threads_matrix);
        require(cfg.pam.num_threads_matrix > 0, p->key_name("num_threads_matrix") + " must be > 0");
    }

    /* ===================== Clara ===================== */
    if (auto c = get_section(tbl, "Clara")) {
        c->check_keys({"sample_size", "sample_fraction", "min_sample_size", "num_samples",
                       "num_threads_assignment", "random_seeds"});
        size_t sample_size = 0;
        c->read("sample_size", sample_size);
        double sample_fraction = 0.0;
        c->read("sample_fraction", sample_fraction);
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
            require(sample_fraction == 0, "Clara.sample_fraction must be in (0, 1]");
            cfg.clara.sample_size = std::monostate{};
        }
        c->read("min_sample_size", cfg.clara.min_sample_size);
        c->read("num_samples", cfg.clara.num_samples);
        c->read("num_threads_assignment", cfg.clara.num_threads_assignment);
        require(cfg.clara.num_samples > 0, c->key_name("num_samples") + " must be > 0");
        require(cfg.clara.num_threads_assignment > 0, c->key_name("num_threads_assignment") + " must be > 0");
        read_seeds(*c, "random_seeds", cfg.clara.random_seeds);
        require(cfg.clara.random_seeds.size() <= cfg.clara.num_samples, 
                "Number of random_seeds exceeds num_samples");
    }
    // Missing seeds are random (also without a [Clara] section: one seed per sample is needed)
    {
        std::random_device rd;
        while (cfg.clara.random_seeds.size() < cfg.clara.num_samples){
            cfg.clara.random_seeds.push_back(rd());
        }
    }

    /* ===================== Clarans ===================== */
    if (auto cr = get_section(tbl, "Clarans")) {
        cr->check_keys({"num_local", "max_neighbors", "num_threads", "random_seed_local_search",
                        "random_seeds_initialization"});
        cr->read("num_local", cfg.clarans.num_local);
        cr->read("max_neighbors", cfg.clarans.max_neighbors);    
        cr->read("num_threads", cfg.clarans.num_threads);
        cr->read("random_seed_local_search", cfg.clarans.random_seed_local_search);
        require(cfg.clarans.num_local > 0, cr->key_name("num_local") + " must be > 0");
        require(cfg.clarans.max_neighbors > 0, cr->key_name("max_neighbors") + " must be > 0");
        require(cfg.clarans.num_threads > 0, cr->key_name("num_threads") + " must be > 0");
        read_seeds(*cr, "random_seeds_initialization", cfg.clarans.random_seeds_initialization);
        require(cfg.clarans.random_seeds_initialization.size() <= cfg.clarans.num_local, 
                "Number of random_seeds_initialization exceeds num_local");
    }
    // Missing seeds are random (also without a [Clarans] section: one seed per start is needed)
    {
        std::random_device rd;
        while (cfg.clarans.random_seeds_initialization.size() < cfg.clarans.num_local){
            cfg.clarans.random_seeds_initialization.push_back(rd());
        }
    }

    /* ===================== IO ===================== */
    auto run_name_opt = read_io_section(tbl, cfg.io);

    /* ===================== Clustering ===================== */
    if (auto cl = get_section(tbl, "Clustering")) {
        cl->check_keys({"k", "parallel", "clustering_algo", "distance_matrix", "compute_missing_distances",
                        "failed_distances"});
        cl->read_required("k", cfg.cluster.k);
        cl->read("parallel", cfg.cluster.parallel);
        cl->read_enum("clustering_algo", cfg.cluster.clustering_algo, parse_clustering_algo);
        cl->read("distance_matrix", cfg.cluster.distance_matrix);
        cl->read("compute_missing_distances", cfg.cluster.compute_missing_distances);
        cl->read_enum("failed_distances", cfg.cluster.failed_distances, parse_failed_distances);
        require(cfg.cluster.k > 0, cl->key_name("k") + " must be > 0");
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
    toml::table tbl = parse_config_file(filename);
    DistanceConfig cfg;  // defaults are set here
    cfg.warnings = check_sections(tbl, {"Gurobi", "IO", "Parallel", "Logging"});

    /* ===================== Gurobi ===================== */
    read_gurobi_section(tbl, cfg.model, cfg.solver);

    /* ===================== IO ===================== */
    auto run_name_opt = read_io_section(tbl, cfg.io);

    /* ==================== Parallel ======================= */
    if (auto par = get_section(tbl, "Parallel")) {
        par->check_keys({"num_threads", "batch_size"});
        par->read("num_threads", cfg.parallel.num_threads_matrix);
        par->read("batch_size", cfg.parallel.default_batch_size);
        require(cfg.parallel.num_threads_matrix > 0, par->key_name("num_threads") + " must be > 0");
        require(cfg.parallel.default_batch_size > 0, par->key_name("batch_size") + " must be > 0");
    }

    /* ===================== Logging ===================== */
    read_logging_section(tbl, cfg.logging);

    // Generate default run_name from config if there was no run_name specified
    cfg.io.run_name = run_name_opt.value_or("default_run");

    return cfg;
}


std::string to_string(ClusteringConfig::FailedDistances mode)
{
    switch (mode) {
        case ClusteringConfig::FailedDistances::Abort:    return "abort";
        case ClusteringConfig::FailedDistances::Infinite: return "infinite";
        case ClusteringConfig::FailedDistances::Exclude:  return "exclude";
    }
    return "unknown";
}

ModelParametersFromFile load_model_parameters(const std::string& filename)
{
    toml::table tbl = parse_config_file(filename);
    ModelParametersFromFile result;
    auto& model = result.model;
    auto g = get_section(tbl, "Gurobi");
    for (auto key : {"cost_factor_angle_difference"}) {
        if (not g or not g->get(key)) result.missing_keys.emplace_back(key);
    }
    if (g) {
        g->read("enforce_cyclic_order", model.enforce_cyclic_order);
        g->read("consider_angle_objective", model.consider_angle_objective);
        g->read("cost_factor_deletion_insertion", model.cost_factor_deletion_insertion);
        g->read("cost_factor_length_difference", model.cost_factor_length_difference);
        g->read("cost_factor_angle_difference", model.cost_factor_angle_difference);
        g->read_enum("angle_type", model.angle_type, parse_angle_type);
    }
    return result;
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
    print_entry(out, "overwrite", tf(io.overwrite));
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
    print_entry(out, "distance_matrix", cluster.distance_matrix.value_or("not set (all distances are computed)"));
    if (cluster.distance_matrix) {
        print_entry(out, "compute_missing_distances", tf(cluster.compute_missing_distances));
    }
    print_entry(out, "failed_distances", to_string(cluster.failed_distances));
    return out;
}

std::ostream& operator<<(std::ostream& out, const ParallelConfig& parallel)
{
    out << "\n[Parallel]\n";
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
