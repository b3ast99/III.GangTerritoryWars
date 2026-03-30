#pragma once
// Stub: minimal plugin.h for unit tests.
#include <cstdlib>

namespace plugin {
    // Deterministic stub: returns a value in [a, b).
    // Tests that need specific randomness control the seed via srand() before calling.
    inline float RandomNumberInRange(float a, float b) {
        if (b <= a) return a;
        return a + (b - a) * (static_cast<float>(std::rand()) / (static_cast<float>(RAND_MAX) + 1.0f));
    }
}
