#ifndef GEOMGMD_DISTANCE_SOLVER_CONFIG_H
#define GEOMGMD_DISTANCE_SOLVER_CONFIG_H

#include "utility/geometry_base.h"

struct GurobiConfig {

    int timelimit_sec = 60;

    bool enforce_cyclic_order = false;
    bool consider_angle_objective = true;

    AngleType angle_type = AngleType::Signed;

    double cost_factor_deletion_insertion = 1.0;
    double cost_factor_length_difference = 1.0;
    double cost_factor_angle_difference = 2.0;
};

#endif // GEOMGMD_DISTANCE_SOLVER_CONFIG_H
