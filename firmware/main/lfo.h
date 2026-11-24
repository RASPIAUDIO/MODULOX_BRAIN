#pragma once

#include "parameter.h"
#include "transport.h"

#include <cstdint>

struct LfoSettings {
    int wave;
    int rate;
    int depth;
    int level;
    bool operator==(const LfoSettings&) const = default;
};

class Lfo {
public:
    enum ParameterIndex { wave, rate, depth, level };
    ModuleParameters controls(const char* name = "LFO") { return {name, parameters_}; }
    LfoSettings settings() const
    {
        return {parameters_[wave].get(), parameters_[rate].get(),
                parameters_[depth].get(), parameters_[level].get()};
    }
    static float waveform(int wave, uint32_t phase);
    static constexpr int max_rate = 400;
    bool synced() const { return parameters_[4].get(); }
    unsigned division() const { return parameters_[5].get(); }
    unsigned phase() const { return parameters_[6].get(); }

private:
    static constexpr const char* waves[] = {"Sine", "Square", "Triangle"};
    static constexpr const char* sync_modes[] = {"Free", "Sync"};
    Parameter parameters_[7] = {
        {"Wave", 0, 2, 1, 0, "", waves},
        {"Rate", 1, max_rate, 1, 20, "x0.05Hz", {}, 12},
        {"Depth", -100, 100, 1, 50, "%"},
        {"Level", -100, 100, 1, 0, "%"},
        {"Sync", 0, 1, 1, 0, "", sync_modes},
        {"Division", 0, 9, 1, 4, "", division_names},
        {"Phase", 0, 359, 1, 0, "deg"},
    };
};

struct Modulation {
    float position;
    float depth;
    float level;

    float apply(float base, float minimum, float maximum, float span) const
    {
        const float center = base + level * span;
        const float excursion = depth * span;
        const float start = std::clamp(center - excursion, minimum, maximum);
        const float end = std::clamp(center + excursion, minimum, maximum);
        return start + (end - start) * position;
    }
};
