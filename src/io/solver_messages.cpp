#include "io/solver_messages.h"

#include "utility/log.h"

void report_solver_failure(DistanceResult const &res)
{
    const std::string message = distance_result_error_message(res);
    if (message.empty()) {
        return;
    }
    geomgmd::log::error(message);
}
