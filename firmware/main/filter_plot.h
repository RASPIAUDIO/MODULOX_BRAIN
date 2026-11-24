#pragma once
#include "filter.h"
#include <cmath>
#include <span>

// Display core only. 96 frequency bins, then linear interpolation to the LCD width.
class FilterPlot {
    static constexpr unsigned bins = 96;
    struct Phase { float c, s, c2, s2; };
    std::array<Phase, bins> phases_{};
public:
    FilterPlot() {
        for (unsigned i = 0; i < bins; ++i) {
            const float hz = 20.0f * std::pow(900.0f, float(i) / (bins - 1));
            const float angle = 6.283185307f * hz / 44100.0f;
            const float c = std::cos(angle), s = std::sin(angle);
            phases_[i] = {c, s, 2 * c * c - 1, 2 * c * s};
        }
    }
    void draw(const Filter::Response& response, std::span<uint8_t> points, int height) const {
        std::array<float, bins> values{};
        for (unsigned i = 0; i < bins; ++i) {
            const auto& p = phases_[i];
            float power = 1;
            for (int stage = 0; stage < std::clamp(response.stages, 0, 2); ++stage) {
                const auto& k = response.coefficients[stage];
                const float re = k[0] + k[1] * p.c + k[2] * p.c2;
                const float im = k[1] * p.s + k[2] * p.s2;
                const float dr = 1 + k[3] * p.c + k[4] * p.c2;
                const float di = k[3] * p.s + k[4] * p.s2;
                power *= (re * re + im * im) / std::max(dr * dr + di * di, 1e-15f);
            }
            const float db = 10 * std::log10(std::max(power, 1e-12f));
            values[i] = std::clamp((24 - db) * (height - 1) / 84, 0.0f, float(height - 1));
        }
        for (unsigned x = 0; x < points.size(); ++x) {
            const float at = float(x) * (bins - 1) / std::max(1u, unsigned(points.size()) - 1);
            const unsigned first = std::min(unsigned(at), bins - 1);
            const float mix = at - first;
            points[x] = std::lround(values[first] + mix * (values[std::min(first + 1, bins - 1)] - values[first]));
        }
    }
};
