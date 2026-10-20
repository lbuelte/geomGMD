// Created by Laura Bülte on 2026-08-20.

#ifndef GEOMGMD_UTILITY_GEOMETRY_BASE_H
#define GEOMGMD_UTILITY_GEOMETRY_BASE_H

#include <ostream>

struct Point {
    const double x;
    const double y;
};


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