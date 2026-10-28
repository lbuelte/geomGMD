#ifndef GEOMGMD_UTILITY_GEOMETRY_BASE_H
#define GEOMGMD_UTILITY_GEOMETRY_BASE_H

#include <ostream>

struct Point {
    const double x;
    const double y;
};


// Angle of a path of length two in the path mapping cost ([Gurobi] angle_type = "signed" / "unsigned"):
// Signed: anti-clockwise enclosed angle in [0, 2π) as in the paper (the name is kept for the config files);
// Unsigned: the smaller angle between the two edges, in [0, π].
enum class AngleType{
        Signed,
        Unsigned
    };

inline std::ostream& operator<<(std::ostream& os, AngleType angle) {
    switch (angle) {
        case AngleType::Signed:
            return os << "Signed";

        case AngleType::Unsigned:
            return os << "Unsigned";
    }
    return os << "UNKNOWN";
}

#endif // GEOMGMD_UTILITY_GEOMETRY_BASE_H