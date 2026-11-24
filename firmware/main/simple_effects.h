#pragma once

#include "effects.h"

class Decimator {
public:
    struct Settings { float rate, mix; };
    struct State { float phase = 1, held = 0, mix = -1; };
    ModuleParameters controls() { return {"Decimator", parameters_}; }
    Settings settings(const Modulation* rate) const
    { return {effect_parameter(parameters_[0], rate) / 44100.f, parameters_[1].get() * .01f}; }
    static void process(const float* input, float* output, unsigned frames, Settings settings, State& state, float scale = 1)
    {
        if (state.mix < 0) state.mix = settings.mix;
        const float step = (settings.mix - state.mix) / frames;
        for (unsigned i = 0; i < frames; ++i) {
            if (state.phase >= 1) { state.phase -= 1; state.held = input[i]; }
            state.phase += settings.rate;
            state.mix += step;
            output[i] = input[i] + (state.held - input[i]) * state.mix;
        }
        state.mix = settings.mix;
    }
private:
    Parameter parameters_[2] = {{"Rate", 100, 44100, 1, 11025, "Hz"}, {"Mix", 0, 100, 1, 100, "%"}};
};

class Bitcrusher {
public:
    struct Settings { float steps, mix; bool bypass; };
    struct State { float mix = -1; };
    ModuleParameters controls() { return {"Bitcrusher", parameters_}; }
    Settings settings(const Modulation* bits) const
    {
        const unsigned resolution = unsigned(std::clamp(std::lround(effect_parameter(parameters_[0], bits)), 2l, 16l));
        return {float(1u << (resolution - 1)), parameters_[1].get() * .01f, resolution == 16};
    }
    static void process(const float* input, float* output, unsigned frames, Settings settings, State& state, float scale = 1)
    {
        if (state.mix < 0) state.mix = settings.mix;
        const float step = (settings.mix - state.mix) / frames;
        const float inverse = 1 / scale;
        for (unsigned i = 0; i < frames; ++i) {
            state.mix += step;
            if (settings.bypass || state.mix == 0) { output[i] = input[i]; continue; }
            const float code = std::clamp(std::round(std::clamp(input[i] * inverse, -1.f, 1.f) * settings.steps),
                -settings.steps, settings.steps - 1);
            const float quantized = code / settings.steps * scale;
            output[i] = input[i] + (quantized - input[i]) * state.mix;
        }
        state.mix = settings.mix;
    }
private:
    Parameter parameters_[2] = {{"Bits", 2, 16, 1, 8}, {"Mix", 0, 100, 1, 100, "%"}};
};

// Linked stereo peak compressor. One gain target per 32-frame block; gain
// attack/release runs per sample. No per-sample log/pow and no lookahead buffer.
class Compressor {
public:
    ModuleParameters controls() { return {"Compressor", parameters_}; }
    void reset() { gain_ = 1; mix_ = -1; }
    void process(float* left, float* right, unsigned frames, const Modulation* threshold_mod = nullptr)
    {
        const float wet = parameters_[5].get() * .01f;
        if (wet == 0) { reset(); mix_ = 0; return; }
        const float threshold = effect_parameter(parameters_[0], threshold_mod);
        if (threshold != last_threshold_) { last_threshold_ = threshold; threshold_ = std::pow(10.f, threshold * .05f) * 32767; }
        const int attack = parameters_[2].get(), release = parameters_[3].get(), makeup = parameters_[4].get();
        if (attack != last_attack_) { last_attack_ = attack; attack_ = std::exp(-1.f / (44.1f * attack)); }
        if (release != last_release_) { last_release_ = release; release_ = std::exp(-1.f / (44.1f * release)); }
        if (makeup != last_makeup_) { last_makeup_ = makeup; makeup_ = std::pow(10.f, makeup * .05f); }
        const float ratio = parameters_[1].get();
        float peak = 0;
        for (unsigned i = 0; i < frames; ++i) peak = std::max(peak, std::max(std::abs(left[i]), std::abs(right[i])));
        const float target = peak > threshold_ && ratio > 1 ? std::pow(threshold_ / peak, 1 - 1 / ratio) : 1;
        const float pole = target < gain_ ? attack_ : release_;
        if (mix_ < 0) mix_ = wet;
        const float mix_step = (wet - mix_) / frames;
        for (unsigned i = 0; i < frames; ++i) {
            gain_ = target + pole * (gain_ - target); mix_ += mix_step;
            const float gain = 1 + (gain_ * makeup_ - 1) * mix_;
            left[i] *= gain; right[i] *= gain;
        }
        mix_ = wet;
    }
private:
    Parameter parameters_[6] = {{"Threshold", -60, 0, 1, -18, "dB"}, {"Ratio", 1, 20, 1, 4, ":1"},
        {"Attack", 1, 100, 1, 10, "ms"}, {"Release", 10, 1000, 10, 100, "ms"},
        {"Makeup", 0, 24, 1, 0, "dB"}, {"Mix", 0, 100, 1, 100, "%"}};
    float gain_ = 1, mix_ = -1, threshold_ = 1, attack_ = 0, release_ = 0, makeup_ = 1, last_threshold_ = 999;
    int last_attack_ = -1, last_release_ = -1, last_makeup_ = -1;
};
