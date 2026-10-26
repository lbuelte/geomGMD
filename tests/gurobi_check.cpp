// Exit code 0 if a Gurobi environment can be started (licence available), otherwise geomgmd_test::skip_code.
// Used by run_app_test.cmake to skip the application tests without a licence.

#include "check.h"
#include "distance/geomGMDSolver.h"

#include <iostream>

int main()
{
    try {
        GurobiEnvManager::getEnv();
        return 0;
    } catch (GRBException const &e) {
        std::cerr << "Gurobi is not available (" << e.getMessage() << ")\n";
        return geomgmd_test::skip_code;
    }
}
