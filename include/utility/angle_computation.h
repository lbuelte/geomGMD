#ifndef GEOMGMD_UTILITY_ANGLE_COMPUTATION_H
#define GEOMGMD_UTILITY_ANGLE_COMPUTATION_H

#include "utility/geometry_base.h"


// Returns the signed angle in radians between the line segments ab and bc, in (-π, π].
// Positive angles are counter-clockwise rotations from ab to bc. Negative angles are clockwise rotations.
// In particular, we distinguish between the input (a,b,c) and (c,b,a). 
// We have here, that angle(a,b,c)=-angle(c,b,a).
long double compute_signed_angle_between_lines(Point const &a, Point const &b, Point const &c);

// Returns the signed angle in radians between the line segments ab and bc, in (0, π].
// This is always the "smaller" angle between the lines.
// In particular, we do not distinguish between the input (a,b,c) and (c,b,a).
long double compute_unsigned_angle_between_lines(Point const &a, Point const &b, Point const &c);


#endif // GEOMGMD_UTILITY_ANGLE_COMPUTATION_H