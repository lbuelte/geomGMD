// Created by Laura Bülte on 2026-04-02.

#include "io/output_context.h"
#include "utility/log.h"

#include <cmath>
#include <sstream>
#include <iomanip>

namespace  {

// Helper functions for writing interval distance matrix with CSVWriter

static constexpr double EPS = 1e-8;

std::string format_double(double value) {
    if (std::isinf(value)) return value > 0 ? "inf" : "-inf";
    if (std::isnan(value)) return "nan";

    std::ostringstream oss;
    if (std::abs(value) >= 1e4) {
        oss << std::scientific << std::setprecision(3) << value;
    } else {
        oss << std::fixed << std::setprecision(3) << value;
    }
    return oss.str();
}

// Relative MIP gap with 3 significant digits in scientific notation (e.g. 1.23e-02); empty if unknown
std::string format_gap(std::optional<double> gap) {
    if (not gap.has_value()) return "";
    if (std::isinf(*gap)) return "inf";
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(2) << *gap;
    return oss.str();
}

bool is_optimal(double lb, double ub) {
    return std::abs(lb - ub) < EPS;
}

} // end of anonymour namespace


// We assume that matrix is symmetric
void OutputContext::write_interval_distance_matrix(const std::vector<std::vector<IntervalDistance>>& matrix)
{
    if (not intervaldistance_matrix_writer.has_value()){
        return;
    }
    intervaldistance_matrix_writer.value().write_row({"row", "col", "lb", "ub", "optimal"});

    for (size_t i = 0; i < matrix.size(); ++i) {
        for (size_t j = i+1; j < matrix[i].size(); ++j) {
            const auto& d = matrix[i][j];

            intervaldistance_matrix_writer.value().write_row({
                    std::to_string(i),
                    std::to_string(j),
                    format_double(d.lb),
                    format_double(d.ub),
                    is_optimal(d.lb, d.ub) ? "true" : "false"
            });
        }
        intervaldistance_matrix_writer.value().flush();
    }    
}

void OutputContext::write_computation_info_row(const GeomGMDResult &result)
{
    if (not pairwise_computation_info_writer.has_value()){
        return;
    }
    
    if (not result.distance.has_value()){
        return;
    }
    const IntervalDistance dist = result.distance.value();
    auto const &run_info = result.instance;
    auto const &statistics = result.statistics;
    pairwise_computation_info_writer.value().write_row({
        std::to_string(run_info.g),
        std::to_string(run_info.h),
        std::to_string(run_info.ng),
        std::to_string(run_info.nh),
        std::to_string(run_info.eg),
        std::to_string(run_info.eh),
        format_double(dist.lb),
        format_double(dist.ub),
        is_optimal(dist.lb, dist.ub) ? "true" : "false",
        std::to_string(result.runtime.count()),
        statistics.status ? gurobi_status_name(*statistics.status) : "",
        format_gap(statistics.mip_gap),
        std::to_string(static_cast<long long>(statistics.node_count))
    });
}