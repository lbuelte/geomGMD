#include "clustering/sampling.h"

#include <random>

std::vector<size_t> uniformly_sample_indices(size_t n, size_t k, std::optional<uint32_t> random_seed){
    std::vector<size_t> reservoir;
    reservoir.reserve(k);

    std::mt19937 rng;
    if (random_seed){
        rng.seed(random_seed.value());
    } else {
        rng.seed(std::random_device{}());
    }

    // Fill the reservoir with the first k elements
    for (size_t i = 0; i < k && i < n; ++i)
        reservoir.push_back(i);

    // Replace elements with gradually decreasing probability
    for (size_t i = k; i < n; ++i) {
        std::uniform_int_distribution<size_t> dist(0, i);
        size_t j = dist(rng);
        if (j < k)
            reservoir[j] = i;
    }

    return reservoir;
}

