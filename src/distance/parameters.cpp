#include "distance/parameters.h"

#include <sstream>

std::string to_string(ModelParameters const &model, SolverSettings const &solver)
{
    std::ostringstream out;
    out << "\nModel parameters\n";
    out << "  cost_factor_deletion_insertion = " << model.cost_factor_deletion_insertion << '\n';
    out << "  cost_factor_length_difference  = " << model.cost_factor_length_difference << '\n';
    out << "  cost_factor_angle_difference   = " << model.cost_factor_angle_difference << '\n';
    out << "  angle_type                     = " << model.angle_type << '\n';
    out << "  consider_angle_objective       = " << model.consider_angle_objective << '\n';
    out << "  enforce_cyclic_order           = " << model.enforce_cyclic_order << '\n';
    out << "Solver settings\n";
    out << "  timelimit_sec                  = " << solver.timelimit_sec << '\n';
    out << "  threads                        = " << solver.threads << '\n';
    out << "  seed                           = " << solver.seed << '\n';
    return out.str();
}
