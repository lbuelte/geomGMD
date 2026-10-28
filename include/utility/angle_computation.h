#ifndef GEOMGMD_UTILITY_ANGLE_COMPUTATION_H
#define GEOMGMD_UTILITY_ANGLE_COMPUTATION_H

#include "utility/geometry_base.h"


// Returns the enclosed angle of the path (a, b, c) in radians, in [0, 2π): the anti-clockwise angle at b from
// the segment ba to the segment bc (paper, Sec. 3.2). We have angle(a,b,c) = 2π - angle(c,b,a) (for angles != 0).
// Used for AngleType::Signed.
long double compute_anticlockwise_angle(Point const &a, Point const &b, Point const &c);

// Returns the angle in radians between the segments ba and bc, in [0, π].
// This is always the "smaller" angle between the segments.
// In particular, we do not distinguish between the input (a,b,c) and (c,b,a).
// Used for AngleType::Unsigned.
long double compute_unsigned_angle_between_lines(Point const &a, Point const &b, Point const &c);


#endif // GEOMGMD_UTILITY_ANGLE_COMPUTATION_H
