#include "utility/angle_computation.h"

#include <cmath>        // std::fabs, std::atan2, std::cos, std::sin
#include <numbers>      // std::numbers::pi_v

namespace {

// Returns the angle in radians from the segment ba to the segment bc, in (-π, π].
// Positive angles are counter-clockwise rotations from ba to bc, negative angles clockwise rotations.
long double angle_minus_pi_to_pi(Point const &a, Point const &b, Point const &c){

    // v1 = a - b (incoming edge)
    long double vx1 = a.x - b.x;
    long double vy1 = a.y - b.y;

    // v2 = c - b (outgoing edge)
    long double vx2 = c.x - b.x;
    long double vy2 = c.y - b.y;

    // angle of v1 relative to x-axis
    long double theta = std::atan2(vy1, vx1);

    // rotate v2 by -theta, so v1 aligns with +x axis
    long double cos_t = std::cos(-theta);
    long double sin_t = std::sin(-theta);
    long double vx2r = vx2 * cos_t - vy2 * sin_t;
    long double vy2r = vx2 * sin_t + vy2 * cos_t;

    // now compute angle of rotated v2
    return std::atan2(vy2r, vx2r); // in (-π, π]
}

} // end of anonymous namespace

long double compute_anticlockwise_angle(Point const &a, Point const &b, Point const &c){
    long double const angle = angle_minus_pi_to_pi(a, b, c);
    // clockwise rotations by |angle| are anti-clockwise rotations by 2π - |angle|
    return angle < 0 ? angle + 2 * std::numbers::pi_v<long double> : angle;
}

long double compute_unsigned_angle_between_lines(Point const &a, Point const &b, Point const &c){
    return std::fabs(angle_minus_pi_to_pi(a, b, c));
}
