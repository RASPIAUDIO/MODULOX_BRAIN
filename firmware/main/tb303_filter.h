#pragma once

#include "effects.h"
#include <array>

// Float adaptation of Robin Schmidt's MIT Open303 TeeBeeFilter TB_303 branch.
// Source: midilab/jc303 305ad279cf864e7954c6db47612645befb36956c.
// Copyright (c) 2009 Robin Schmidt. Full notice: ../third_party/open303/LICENSE.
// Does not include the separate dfl diode ladder or the complete 303 voice.
struct TeeBee303 {
    struct Coefficients { float b0 = 0, k = 0, g = 1; };
    struct State { float y1 = 0, y2 = 0, y3 = 0, y4 = 0, hp_x = 0, hp_y = 0; };
    static constexpr unsigned oversampling = 4;
    static constexpr float rate = 44100.f * oversampling;
    static Coefficients coefficients(float cutoff, float resonance)
    {
        const float r = (1 - std::exp(-.03f * std::clamp(resonance, 0.f, 95.f))) / (1 - std::exp(-3.f));
        const float fx = std::clamp(cutoff, 200.f, 20000.f) * .7071067811865475f / rate;
        const float b0 = (.00045522346f + 6.1922189f * fx) / (1 + 12.358354f * fx + 4.4156345f * fx * fx);
        const float k = fx * (fx * (fx * (fx * (fx * (fx + 7198.6997f) - 5837.7917f) - 476.47308f) + 614.95611f) + 213.87126f) + 16.998792f;
        return {b0, k * r, ((k / 17 - 1) * r + 1) * (1 + r)};
    }
    static float next(float input, Coefficients c, State& s)
    {
        // 150 Hz one-pole highpass in the feedback path (Open303 default).
        constexpr float hp_a = .99467140296f; // exp(-2*pi*150/176400)
        constexpr float hp_b = .5f * (1 + hp_a);
        const float feedback = c.k * s.y4;
        s.hp_y = hp_b * (feedback - s.hp_x) + hp_a * s.hp_y; s.hp_x = feedback;
        const float y0 = input - s.hp_y;
        s.y1 += 2 * c.b0 * (y0 - s.y1 + s.y2);
        s.y2 += c.b0 * (s.y1 - 2 * s.y2 + s.y3);
        s.y3 += c.b0 * (s.y2 - 2 * s.y3 + s.y4);
        s.y4 += c.b0 * (s.y3 - 2 * s.y4);
        if (!(s.y4 > -32 && s.y4 < 32)) { s = {}; return 0; }
        return 2 * c.g * s.y4;
    }
};

class Tb303Filter {
public:
    struct Biquad { float z1 = 0, z2 = 0; };
    struct State {
        TeeBee303::State filter;
        TeeBee303::Coefficients current;
        std::array<Biquad, 2> lowpass{};
        float previous = 0, mix = -1;
    };
    struct Settings { TeeBee303::Coefficients coefficients; float drive, mix; };
    ModuleParameters controls() { return {"TB-303", parameters_}; }
    Settings settings(const Modulation* cutoff, const Modulation* resonance)
    {
        const float hz = effect_parameter(parameters_[0], cutoff), res = effect_parameter(parameters_[1], resonance);
        if (hz != last_cutoff_ || res != last_resonance_) {
            last_cutoff_ = hz; last_resonance_ = res; cached_ = TeeBee303::coefficients(hz, res);
        }
        const int drive = parameters_[2].get();
        if (drive != last_drive_) { last_drive_ = drive; drive_ = std::pow(10.f, drive * .05f); }
        return {cached_, drive_, parameters_[3].get() * .01f};
    }
    static void process(const float* input, float* output, unsigned frames, Settings settings, State& state, float scale = 1)
    {
        if (settings.mix == 0) { std::copy_n(input, frames, output); state = {}; return; }
        if (state.mix < 0) { state.mix = settings.mix; state.current = settings.coefficients; }
        const TeeBee303::Coefficients step{(settings.coefficients.b0 - state.current.b0) / frames,
            (settings.coefficients.k - state.current.k) / frames, (settings.coefficients.g - state.current.g) / frames};
        const float wet_step = (settings.mix - state.mix) / frames, inverse = 1 / scale;
        for (unsigned i = 0; i < frames; ++i) {
            state.current.b0 += step.b0; state.current.k += step.k; state.current.g += step.g; state.mix += wet_step;
            const float dry = input[i];
            float driven = dry * inverse * settings.drive;
            if (settings.drive > 1) driven /= 1 + std::abs(driven); // External drive; absent in the original TB_303 branch.
            driven = std::clamp(driven, -4.f, 4.f);
            const float increment = (driven - state.previous) * .25f;
            float x = state.previous, filtered = 0;
            for (unsigned sub = 0; sub < TeeBee303::oversampling; ++sub) {
                x += increment;
                filtered = TeeBee303::next(x, state.current, state.filter);
                // Fourth-order 18 kHz Butterworth anti-alias filter at 176.4 kHz.
                constexpr float coefficients[2][5] = {
                    {.0639535845f, .1279071690f, .0639535845f, -1.0323623253f, .2881766633f},
                    {.0807992845f, .1615985690f, .0807992845f, -1.3042918217f, .6274889596f}};
                for (unsigned stage = 0; stage < 2; ++stage) {
                    const auto* c = coefficients[stage]; auto& s = state.lowpass[stage];
                    const float y = c[0] * filtered + s.z1;
                    s.z1 = c[1] * filtered - c[3] * y + s.z2; s.z2 = c[2] * filtered - c[4] * y; filtered = y;
                }
            }
            state.previous = driven;
            output[i] = dry + (std::clamp(filtered, -8.f, 8.f) * scale - dry) * state.mix;
        }
        state.current = settings.coefficients; state.mix = settings.mix;
    }
private:
    Parameter parameters_[4] = {{"Cutoff", 200, 20000, 1, 1000, "Hz", {}, 20},
        {"Reso", 0, 95, 1, 35, "%"}, {"Drive", 0, 24, 1, 0, "dB"}, {"Mix", 0, 100, 1, 100, "%"}};
    TeeBee303::Coefficients cached_;
    float last_cutoff_ = -1, last_resonance_ = -1, drive_ = 1;
    int last_drive_ = -1;
};
