#ifndef GEOMGMD_DISTANCE_PARAMETERS_H
#define GEOMGMD_DISTANCE_PARAMETERS_H

#include "utility/geometry_base.h"

#include <ostream>

// What is computed: parameters of the geomGMD model (paper: c_id, c_m, c_r).
struct ModelParameters {
    double cost_factor_deletion_insertion = 1.0;   // c_id
    double cost_factor_length_difference = 1.0;    // c_m
    double cost_factor_angle_difference = 2.0;     // c_r

    AngleType angle_type = AngleType::Signed;
    bool consider_angle_objective = true;
    bool enforce_cyclic_order = false;
};

// How it is computed: settings of the Gurobi solver.
struct SolverSettings {
    int timelimit_sec = 60;
    int threads = 1;
    int seed = 1;
};

// Listing of the parameters / settings for the log, one "  key = value" line each (keys as in the
// config file, without section header).
std::ostream& operator<<(std::ostream& out, ModelParameters const &model);
std::ostream& operator<<(std::ostream& out, SolverSettings const &solver);

#endif // GEOMGMD_DISTANCE_PARAMETERS_H
