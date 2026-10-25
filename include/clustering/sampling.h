#ifndef GEOMGMD_CLUSTERING_SAMPLING_H
#define GEOMGMD_CLUSTERING_SAMPLING_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// k of the indices 0, ..., n-1, drawn uniformly without replacement (reservoir sampling; all n if k >= n).
// With a seed, the sample is reproducible.
std::vector<size_t> uniformly_sample_indices(size_t n, size_t k, std::optional<uint32_t> random_seed);

#endif // GEOMGMD_CLUSTERING_SAMPLING_H