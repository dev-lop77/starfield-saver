#pragma once
// Small shared helpers: timing, RNG, math. Header-only to keep things light.

#include <cstdint>
#include <random>

namespace ssaver {

// --- Random number generation -------------------------------------------------
// A single thread-local engine. Seeded from a real entropy source so each run
// (and the "random" events) differ. Note: Date.now()/random restrictions in the
// agent harness do not apply to the compiled C++ program itself.
inline std::mt19937& rng() {
    static thread_local std::mt19937 engine{std::random_device{}()};
    return engine;
}

// Uniform float in [lo, hi).
inline float frand(float lo = 0.0f, float hi = 1.0f) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng());
}

// Uniform int in [lo, hi] (inclusive).
inline int irand(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng());
}

// Returns true with probability p.
inline bool chance(float p) { return frand() < p; }

// --- Math ----------------------------------------------------------------------
inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// Smoothstep easing, handy for enter/exit fades of events.
inline float smoothstep(float t) {
    t = clampf(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace ssaver
