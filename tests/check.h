#ifndef GEOMGMD_TESTS_CHECK_H
#define GEOMGMD_TESTS_CHECK_H

// Minimal checks for the test executables: a failed check prints file, line and the expression and is
// counted; the test's main returns check_result() (1 if a check failed, otherwise 0).
// Tests that need a Gurobi licence return skip_code when none is available (CTest: SKIP_RETURN_CODE).

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

namespace geomgmd_test {

inline int failures = 0;
constexpr int skip_code = 77;

inline void fail(char const *file, int line, char const *what)
{
    ++failures;
    std::cerr << file << ":" << line << ": check failed: " << what << "\n";
}

// a and b are equal up to a relative tolerance (absolute for |b| < 1)
inline bool near(double a, double b, double tol)
{
    return std::abs(a - b) <= tol * std::max(1.0, std::abs(b));
}

inline int check_result()
{
    if (failures > 0) std::cerr << failures << " check(s) failed\n";
    return failures > 0 ? 1 : 0;
}

} // namespace geomgmd_test

#define CHECK(cond) \
    do { if (not (cond)) geomgmd_test::fail(__FILE__, __LINE__, #cond); } while (false)

#define CHECK_NEAR(a, b, tol) \
    do { \
        if (not geomgmd_test::near((a), (b), (tol))) { \
            geomgmd_test::fail(__FILE__, __LINE__, #a " == " #b " (within " #tol ")"); \
            std::cerr << std::setprecision(std::numeric_limits<double>::max_digits10) \
                      << "    values: " << (a) << " vs. " << (b) << "\n"; \
        } \
    } while (false)

// Checks that the statement throws an exception of the given type
#define CHECK_THROWS(statement, exception_type) \
    do { \
        bool thrown = false; \
        try { statement; } catch (exception_type const &) { thrown = true; } catch (...) {} \
        if (not thrown) geomgmd_test::fail(__FILE__, __LINE__, #statement " throws " #exception_type); \
    } while (false)

#endif // GEOMGMD_TESTS_CHECK_H
