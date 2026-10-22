#include "distance/parameters.h"

#include "utility/listing.h"

using geomgmd::listing::print_entry;
using geomgmd::listing::tf;

std::ostream& operator<<(std::ostream& out, ModelParameters const &model)
{
    print_entry(out, "enforce_cyclic_order", tf(model.enforce_cyclic_order));
    print_entry(out, "consider_angle_objective", tf(model.consider_angle_objective));
    print_entry(out, "cost_factor_deletion_insertion", model.cost_factor_deletion_insertion);
    print_entry(out, "cost_factor_length_difference", model.cost_factor_length_difference);
    print_entry(out, "cost_factor_angle_difference", model.cost_factor_angle_difference);
    print_entry(out, "angle_type", model.angle_type);
    return out;
}

std::ostream& operator<<(std::ostream& out, SolverSettings const &solver)
{
    print_entry(out, "timelimit_sec", solver.timelimit_sec);
    print_entry(out, "threads", solver.threads);
    print_entry(out, "seed", solver.seed);
    print_entry(out, "mip_gap", solver.mip_gap);
    return out;
}
