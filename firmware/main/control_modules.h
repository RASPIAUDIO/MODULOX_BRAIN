#pragma once

#include "lfo.h"
#include <initializer_list>

class ControlModule {
public:
    ModuleParameters controls(const char* name) { return {name, {parameters_, count_}}; }
    float get(unsigned index) const { return parameters_[index].get(); }
    struct State {
        float value = 0;
        uint32_t random = 0x36a7b125u;
    };
    void reset(State& state, int kind) const
    {
        state = {};
        if (kind == 0) state.value = get(0) * 0.01f;
    }
    float process(State& state, int kind, float input, bool trigger, unsigned frames, unsigned sample_rate) const
    {
        if (kind == 0) {
            if (trigger) state.value = input;
        } else if (kind == 1) {
            if (trigger) {
                state.random ^= state.random << 13;
                state.random ^= state.random >> 17;
                state.random ^= state.random << 5;
                state.value = (state.random >> 8) * (2.0f / 16777216) - 1;
            }
            return std::clamp(state.value * get(0) * 0.01f + get(1) * 0.01f, -1.0f, 1.0f);
        } else if (kind == 2) {
            state.value = std::clamp(input * get(0) * 0.01f + get(1) * 0.01f, -1.0f, 1.0f);
        } else if (kind == 3) {
            const float ms = get(input > state.value ? 0 : 1);
            const float amount = ms > 0 ? frames * 2000.0f / (ms * sample_rate) : 2;
            state.value += std::clamp(input - state.value, -amount, amount);
        }
        return state.value;
    }

    static ControlModule clock() { return ControlModule({{"Division", 0, 9, 1, 2, "", division_names}}); }
    static ControlModule hold() { return ControlModule({{"Initial", -100, 100, 1, 0, "%"}}); }
    static ControlModule random() { return ControlModule({{"Depth", 0, 100, 1, 100, "%"}, {"Offset", -100, 100, 1, 0, "%"}}); }
    static ControlModule gain() { return ControlModule({{"Gain", -200, 200, 1, 100, "%"}, {"Offset", -100, 100, 1, 0, "%"}}); }
    static ControlModule slew() { return ControlModule({{"Rise", 0, 5000, 1, 100, "ms"}, {"Fall", 0, 5000, 1, 100, "ms"}}); }

private:
    ControlModule(std::initializer_list<Parameter> params) : count_(params.size())
    {
        unsigned i = 0;
        for (const auto& param : params) {
            // Atomic Parameter objects cannot be copied.
            parameters_[i].name = param.name;
            parameters_[i].minimum = param.minimum;
            parameters_[i].maximum = param.maximum;
            parameters_[i].initial = param.initial;
            parameters_[i].unit = param.unit;
            parameters_[i].choices = param.choices;
            parameters_[i].set(param.initial);
            ++i;
        }
    }
    Parameter parameters_[2] = {{"", 0, 1, 1, 0}, {"", 0, 1, 1, 0}};
    unsigned count_;
};
