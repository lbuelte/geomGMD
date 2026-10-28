// Angles of paths of length two: anti-clockwise enclosed angle in [0, 2π) (paper, Sec. 3.2) and unsigned angle.

#include "check.h"
#include "utility/angle_computation.h"

#include <cmath>
#include <numbers>

int main()
{
    double const pi = std::numbers::pi;
    Point const b{0, 0};

    // Anti-clockwise from ba to bc
    CHECK_NEAR(compute_anticlockwise_angle({1, 0}, b, {0, 1}), pi / 2, 1e-12);
    CHECK_NEAR(compute_anticlockwise_angle({0, 1}, b, {1, 0}), 3 * pi / 2, 1e-12);
    CHECK_NEAR(compute_anticlockwise_angle({1, 0}, b, {-1, 0}), pi, 1e-12);       // straight path
    CHECK_NEAR(compute_anticlockwise_angle({1, 0}, b, {2, 0}), 0.0, 1e-12);       // folded back
    // Nearly straight paths bending to different sides: 179° and 181°, not 179° and -179°
    double const d = pi / 180;
    CHECK_NEAR(compute_anticlockwise_angle({1, 0}, b, {std::cos(179 * d), std::sin(179 * d)}), 179 * d, 1e-12);
    CHECK_NEAR(compute_anticlockwise_angle({1, 0}, b, {std::cos(181 * d), std::sin(181 * d)}), 181 * d, 1e-12);
    // Independent of position and orientation: the same path translated and rotated by 90°
    CHECK_NEAR(compute_anticlockwise_angle({5, 4}, {5, 3}, {4, 3}), pi / 2, 1e-12);
    // angle(a, b, c) = 2π - angle(c, b, a)
    CHECK_NEAR(compute_anticlockwise_angle({3, 1}, b, {-1, 2}) + compute_anticlockwise_angle({-1, 2}, b, {3, 1}),
               2 * pi, 1e-12);

    // Unsigned: the smaller angle, in [0, π], the same for both directions
    CHECK_NEAR(compute_unsigned_angle_between_lines({1, 0}, b, {0, 1}), pi / 2, 1e-12);
    CHECK_NEAR(compute_unsigned_angle_between_lines({0, 1}, b, {1, 0}), pi / 2, 1e-12);
    CHECK_NEAR(compute_unsigned_angle_between_lines({1, 0}, b, {-1, 0}), pi, 1e-12);

    return geomgmd_test::check_result();
}
