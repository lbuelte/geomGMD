// Strict parsing of the distance config (load_dist_config): defaults are the paper settings; unknown keys,
// wrong types and values out of range are errors. The config files are written to the working directory.

#include "check.h"
#include "io/config.h"

#include <fstream>
#include <stdexcept>
#include <string>

namespace {

std::string const minimal_io = "[IO]\ndataset = \"tiny\"\ninput_path = \"in\"\noutput_path = \"out\"\n";

// Writes the config text to a file and returns its name
std::string write_config(std::string const &name, std::string const &text)
{
    std::string const file = "test_config_" + name + ".toml";
    std::ofstream(file) << text;
    return file;
}

DistanceConfig load(std::string const &name, std::string const &text)
{
    return load_dist_config(write_config(name, text));
}

} // end of anonymous namespace

int main()
{
    // Defaults: the settings of the paper
    {
        auto const cfg = load("defaults", minimal_io);
        CHECK(cfg.model == ModelParameters{});
        CHECK(cfg.model.cost_factor_deletion_insertion == 1.0);
        CHECK(cfg.model.cost_factor_length_difference == 1.0);
        CHECK(cfg.model.cost_factor_angle_difference == 1.0);
        CHECK(cfg.model.angle_type == AngleType::Signed);
        CHECK(cfg.model.consider_angle_objective);
        CHECK(not cfg.model.enforce_cyclic_order);
        CHECK(cfg.solver.timelimit_sec == 300);
        CHECK(cfg.io.dataset == "tiny");
        CHECK(cfg.io.run_name == "default_run");
        CHECK(cfg.warnings.empty());
    }

    // Values are read
    {
        auto const cfg = load("values", "[Gurobi]\ntimelimit_sec = 10\ncost_factor_angle_difference = 2.5\n"
                                        "angle_type = \"unsigned\"\n" + minimal_io + "run_name = \"r\"\n");
        CHECK(cfg.solver.timelimit_sec == 10);
        CHECK(cfg.model.cost_factor_angle_difference == 2.5);
        CHECK(cfg.model.angle_type == AngleType::Unsigned);
        CHECK(cfg.io.run_name == "r");
    }

    // Sections of other programs are ignored with a warning
    CHECK(load("other_section", minimal_io + "[Clustering]\nk = 3\n").warnings.size() == 1);

    // Errors
    CHECK_THROWS(load("no_io", "[Gurobi]\ntimelimit_sec = 10\n"), std::runtime_error);
    CHECK_THROWS(load("missing_key", "[IO]\ndataset = \"tiny\"\ninput_path = \"in\"\n"), std::runtime_error);
    CHECK_THROWS(load("unknown_key", "[Gurobi]\ntimelimt_sec = 10\n" + minimal_io), std::runtime_error);
    CHECK_THROWS(load("unknown_section", minimal_io + "[Gurobbi]\n"), std::runtime_error);
    CHECK_THROWS(load("wrong_type", "[Gurobi]\ntimelimit_sec = \"10\"\n" + minimal_io), std::runtime_error);
    CHECK_THROWS(load("negative", "[Gurobi]\ncost_factor_length_difference = -1.0\n" + minimal_io),
                 std::runtime_error);
    CHECK_THROWS(load("zero_threads", minimal_io + "[Parallel]\nnum_threads = 0\n"), std::runtime_error);
    CHECK_THROWS(load("angle_type", "[Gurobi]\nangle_type = \"sideways\"\n" + minimal_io), std::runtime_error);
    CHECK_THROWS(load("syntax", "[IO\n"), std::runtime_error);
    CHECK_THROWS(load_dist_config("test_config_does_not_exist.toml"), std::runtime_error);

    return geomgmd_test::check_result();
}
