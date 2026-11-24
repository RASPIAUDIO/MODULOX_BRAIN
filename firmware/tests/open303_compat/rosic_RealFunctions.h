#pragma once
// Host-only math shim for the unchanged upstream TeeBee/OnePole files.
// The embedded firmware compiles only the float adaptation in main/.
#include <cmath>
#include <cassert>
#define INLINE inline
#define DEBUG_BREAK assert(false)
inline constexpr double PI = 3.14159265358979323846;
inline constexpr double SQRT2 = 1.41421356237309504880;
inline constexpr double ONE_OVER_SQRT2 = 0.70710678118654752440;
inline constexpr double TINY = 0; // Denormal injection is irrelevant to the reference comparison.
inline double dB2amp(double db) { return std::pow(10.0, db * .05); }
inline double clip(double x, double lo, double hi) { return x < lo ? lo : x > hi ? hi : x; }
inline void sinCos(double x, double* s, double* c) { *s = std::sin(x); *c = std::cos(x); }
