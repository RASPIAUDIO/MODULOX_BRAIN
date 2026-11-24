#pragma once

#include "lfo.h"
#include "osc.h"
#include <cmath>

class Noise {
public:
    ModuleParameters controls() { return {"Noise", parameters_}; }
    float level(const Modulation* mod) const
    {
        const float base = parameters_[0].get();
        return (mod ? mod->apply(base, 0, 100, 100) : base) * 0.01f;
    }
    static float next(uint32_t& state)
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return int32_t(state) * (1.0f / 2147483648.0f);
    }
private:
    Parameter parameters_[1] = {{"Level", 0, 100, 1, 100, "%"}};
};

class Sub {
public:
    ModuleParameters controls() { return {"Sub", parameters_}; }
    Osc::Settings settings(float pitch_cents = 0, const Modulation* level = nullptr) const
    {
        const float base = parameters_[2].get();
        return {parameters_[0].get(), nullptr,
                Osc::cents_ratio(pitch_cents) * (parameters_[1].get() ? 0.25f : 0.5f),
                (level ? level->apply(base, 0, 100, 100) : base) * 0.01f, 50};
    }
private:
    static constexpr const char* waves[] = {"Sine", "Square", "Triangle"};
    static constexpr const char* octaves[] = {"-1", "-2"};
    Parameter parameters_[3] = {{"Wave", 0, 2, 1, 1, "", waves},
        {"Octave", 0, 1, 1, 0, "", octaves}, {"Level", 0, 100, 1, 100, "%"}};
};

class Vca {
public:
    ModuleParameters controls() { return {"VCA", parameters_}; }
    float gain(const Modulation* cv, float velocity) const
    {
        const float amount = parameters_[1].get() * 0.01f;
        const float control = cv ? cv->apply(0, 0, 1, 1) : 1;
        return parameters_[0].get() * 0.01f * (1 + amount * (control - 1)) * velocity;
    }
private:
    Parameter parameters_[2] = {{"Level", 0, 100, 1, 100, "%"}, {"Depth", 0, 100, 1, 100, "%"}};
};

class RingMod {
public:
    ModuleParameters controls() { return {"Ring Mod", parameters_}; }
    float mix(const Modulation* cv) const
    {
        const float base = parameters_[0].get();
        return (cv ? cv->apply(base, 0, 100, 100) : base) * 0.01f;
    }
private:
    Parameter parameters_[1] = {{"Mix", 0, 100, 1, 100, "%"}};
};

class Wavefolder {
public:
    struct Settings { float drive, symmetry, mix; };
    struct State { float input = 0, output = 0; };
    ModuleParameters controls() { return {"Wavefolder", parameters_}; }
    Settings settings(const Modulation* drive, const Modulation* symmetry, const Modulation* mix) const
    {
        const auto read = [this](unsigned i, const Modulation* cv) {
            const auto& p = parameters_[i];
            return cv ? cv->apply(p.get(), p.minimum, p.maximum, p.maximum - p.minimum) : float(p.get());
        };
        return {1 + read(0, drive) * 0.15f, read(1, symmetry) * 0.01f, read(2, mix) * 0.01f};
    }
    static float fold(float x)
    {
        const float t = x + 1;
        return 1 - std::abs(t - 4 * std::floor(t * 0.25f) - 2);
    }
    static float process(float input, Settings settings, State& state)
    {
        if (settings.mix == 0) { state = {}; return input; }
        const float folded = fold(input * settings.drive + settings.symmetry);
        // Remove DC introduced by asymmetric folding (20 Hz at 44.1 kHz).
        const float blocked = folded - state.input + 0.9971545f * state.output;
        state = {folded, blocked};
        return input + settings.mix * (blocked - input);
    }
private:
    Parameter parameters_[3] = {{"Drive", 0, 100, 1, 0, "%"},
        {"Symmetry", -100, 100, 1, 0, "%"}, {"Mix", 0, 100, 1, 100, "%"}};
};
