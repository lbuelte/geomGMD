#ifndef GEOMGMD_IO_SOLVER_MESSAGES_H
#define GEOMGMD_IO_SOLVER_MESSAGES_H

#include "geomGMDSolver.h"

// Reports why a geomGMD computation did not return a distance (Infeasible, SolverError, SolverException).
// Does nothing for results without an error message.
void report_solver_failure(DistanceResult const &res);

#endif // GEOMGMD_IO_SOLVER_MESSAGES_H
