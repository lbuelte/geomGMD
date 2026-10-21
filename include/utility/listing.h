#ifndef GEOMGMD_UTILITY_LISTING_H
#define GEOMGMD_UTILITY_LISTING_H

// Helpers for listings of parameters in the log: one "  key = value" line per entry, keys aligned.

#include <ostream>
#include <string>
#include <string_view>

namespace geomgmd::listing {

inline const char* tf(bool b) { return b ? "true" : "false"; }

template<typename T>
void print_entry(std::ostream& out, std::string_view key, const T& value)
{
    constexpr size_t key_width = 30;
    out << "  " << key << std::string(key.size() < key_width ? key_width - key.size() : 0, ' ')
        << " = " << value << '\n';
}

} // namespace geomgmd::listing

#endif // GEOMGMD_UTILITY_LISTING_H
