#pragma once

#include "parameter.h"

#include <array>

class Adsr {
public:
    using Settings = std::array<int, 4>;
    ModuleParameters controls() { return {"ADSR", std::span{parameters_}.first<4>()}; }
    ModuleParameters modulation_controls(const char* name) { return {name, parameters_}; }
    float depth() const { return parameters_[4].get() * 0.01f; }
    Settings settings() const;

    class Voice {
    public:
        explicit Voice(unsigned sample_rate) : sample_rate_(sample_rate) {}
        void update_parameters(const Settings& settings);
        void gate_on();
        void gate_off();
        void reset() { stage_ = Stage::idle; samples_left_ = 0; value_ = 0.0f; }
        bool idle() const { return stage_ == Stage::idle; }

        float next()
        {
            if (samples_left_ != 0) {
                value_ += increment_;
                if (--samples_left_ == 0) {
                    value_ = target_;
                    advance();
                }
            }
            return value_;
        }

    private:
        enum class Stage { idle, attack, decay, sustain, release };

        void ramp(float target, unsigned milliseconds);
        void advance();

        const unsigned sample_rate_;
        Stage stage_ = Stage::idle;
        Settings settings_{};
        float value_ = 0.0f;
        float target_ = 0.0f;
        float increment_ = 0.0f;
        unsigned samples_left_ = 0;
    };

private:
    enum Index { attack, decay, sustain, release };
    Parameter parameters_[5] = {
        {"Attack", 0, 2000, 5, 5, "ms"},
        {"Decay", 0, 2000, 10, 120, "ms"},
        {"Sustain", 0, 100, 1, 100, "%"},
        {"Release", 0, 4000, 10, 80, "ms"},
        {"Depth", -100, 100, 1, 50, "%"},
    };
};
