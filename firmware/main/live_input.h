#pragma once

#include "parameter.h"

#include <cstdint>

// The left ADC channel is the microphone path used by Recording. This source
// runs once in the shared graph, in PCM units, independently of MIDI voices.
class LiveInput {
public:
    ModuleParameters controls() { return {"INPUT", {&level_, 1}}; }
    void reset() { gain_ = -1; }
    void render(const int16_t* stereo, float* output, unsigned frames)
    {
        const float target = level_.get() * .01f;
        if (gain_ < 0) gain_ = target;
        const float step = (target - gain_) / frames;
        for (unsigned i = 0; i < frames; ++i) {
            gain_ += step;
            output[i] = stereo ? stereo[2 * i] * gain_ : 0;
        }
        gain_ = target;
        output[frames] = output[frames - 1]; // ESP-DSP's biquad lookahead guard.
    }
private:
    Parameter level_{"Level", 0, 100, 1, 100, "%"};
    float gain_ = -1;
};
