// Created by Laura Bülte on 2026-04-27.

#include "geometry_helpers.h"

#include <cmath>        // std::fabs, std::atan2, std::cos, std::sin    

// Returns the signed angle in radians between the line segments ab and bc, in (-π, π].
// Positive angles are counter-clockwise rotations from ab to bc. Negative angles are clockwise rotations.
// In particular, we distinguish between the input (a,b,c) and (c,b,a). 
// We have here, that angle(a,b,c)=-angle(c,b,a).
long double compute_signed_angle_between_lines(Point const &a, Point const &b, Point const &c){

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
    long double angle = std::atan2(vy2r, vx2r); // in (-π, π]

    return angle;
}

// Returns the signed angle in radians between the line segments ab and bc, in (0, π].
// This is always the "smaller" angle between the lines.
// In particular, we do not distinguish between the input (a,b,c) and (c,b,a).
long double compute_unsigned_angle_between_lines(Point const &a, Point const &b, Point const &c){
    return std::fabs(compute_signed_angle_between_lines(a, b, c));
}
