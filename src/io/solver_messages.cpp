#include "io/solver_messages.h"

#include "utility/log.h"

#include <algorithm>
#include <cmath>

void report_solver_failure(DistanceResult const &res)
{
    const std::string message = distance_result_error_message(res);
    if (message.empty()) {
        return;
    }
    geomgmd::log::error(message);
}

bool gurobi_log_wanted(LoggingConfig const &logging, GeometricGraph const &G, GeometricGraph const &H,
                       DistanceResult const &res)
{
    if (not logging.gurobi_log) {
        return false;
    }
    const bool has_filter = not logging.gurobi_log_pairs.empty() or logging.gurobi_log_min_gap.has_value();
    if (not has_filter) {
        return true;
    }

    const size_t g = static_cast<size_t>(G.graph_id());
    const size_t h = static_cast<size_t>(H.graph_id());
    const bool selected_pair = std::any_of(
        logging.gurobi_log_pairs.begin(), logging.gurobi_log_pairs.end(),
        [&](auto const &pair) {
            return (pair.first == g and pair.second == h) or (pair.first == h and pair.second == g);
        });
    if (selected_pair) {
        return true;
    }

    const auto dist = evaluate_distance_result(res);
    if (not dist.has_value()) {
        // Errors are always written; an expected CutoffHit (e.g. in CLARA) only for selected pairs
        return not distance_result_error_message(res).empty();
    }
    if (logging.gurobi_log_min_gap.has_value()) {
        // Relative gap as defined by Gurobi: |ub - lb| / |ub|
        const double gap = (dist->ub == dist->lb) ? 0.0
                         : std::abs(dist->ub - dist->lb) / std::max(std::abs(dist->ub), 1e-10);
        return gap > *logging.gurobi_log_min_gap;
    }
    return false;
}

void report_gurobi_log(LoggingConfig const &logging, GeometricGraph const &G, GeometricGraph const &H,
                       DistanceResult const &res, std::vector<std::string> const &lines)
{
    if (lines.empty() or not gurobi_log_wanted(logging, G, H, res)) {
        return;
    }
    const std::string prefix = "[" + std::to_string(G.graph_id()) + "," + std::to_string(H.graph_id()) + "] ";
    std::string block;
    for (auto const &line : lines) {
        if (not block.empty()) {
            block += '\n';
        }
        block += prefix + line;
    }
    // One message for the whole log of the pair, so that lines of different pairs do not interleave
    geomgmd::log::debug(block);
}

void report_gurobi_log(LoggingConfig const &logging, GeometricGraph const &G, GeometricGraph const &H,
                       GeomGMDResult const &result)
{
    report_gurobi_log(logging, G, H, result.result, result.gurobi_log);
}
