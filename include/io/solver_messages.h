#ifndef GEOMGMD_IO_SOLVER_MESSAGES_H
#define GEOMGMD_IO_SOLVER_MESSAGES_H

#include "distance/geomGMD.h"
#include "io/config.h"

#include <string>
#include <vector>

// Reports why a geomGMD computation did not return a distance (Infeasible, SolverError, SolverException).
// Does nothing for results without an error message.
void report_solver_failure(DistanceResult const &res);

// True if Gurobi's log of the pair (G, H) with result res should be written according to the
// [Logging] gurobi_log settings (see LoggingConfig).
bool gurobi_log_wanted(LoggingConfig const &logging, GeometricGraph const &G, GeometricGraph const &H,
                       DistanceResult const &res);

// Writes the captured Gurobi log lines of the pair (G, H) at level debug, each prefixed with "[g,h] "
// (graph ids), if gurobi_log_wanted(...) is true.
void report_gurobi_log(LoggingConfig const &logging, GeometricGraph const &G, GeometricGraph const &H,
                       DistanceResult const &res, std::vector<std::string> const &lines);

// Same for the result of compute_geomGMD (uses its result and captured Gurobi log).
void report_gurobi_log(LoggingConfig const &logging, GeometricGraph const &G, GeometricGraph const &H,
                       GeomGMDResult const &result);

#endif // GEOMGMD_IO_SOLVER_MESSAGES_H
