#ifndef GEOMGMD_UTILITY_OVERLOADED_H
#define GEOMGMD_UTILITY_OVERLOADED_H

// Helper for std::visit with one lambda per alternative of a std::variant.
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif // GEOMGMD_UTILITY_OVERLOADED_H
