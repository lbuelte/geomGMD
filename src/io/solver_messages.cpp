#include "io/solver_messages.h"

#include "io/logger.h"

#include <iostream>

void report_solver_failure(DistanceResult const &res)
{
    const std::string message = distance_result_error_message(res);
    if (message.empty()) {
        return;
    }
    std::cout << message << "\n";
    global_log << message << "\n";
}
