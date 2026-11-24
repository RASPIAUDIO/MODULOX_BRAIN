#pragma once

#include "adsr.h"
#include "effects.h"
#include "osc.h"
#include <bit>

// Four sine operators and the eight OPN-style routings, with a deliberately simple ADSR editor.
// This is a new engine, not register-level YM2612 or DX7 emulation.
class Fm4 {
public:
    enum Index { algorithm, depth, feedback, level, pitch, fine, operators };
    enum OpIndex { ratio, op_level, attack, decay, sustain, release, op_parameters };
    static constexpr const char* algorithms[] = {
        "1>2>3>4", "(1+2)>3>4", "(1>2 + 3)>4", "(1 + 2>3)>4",
        "(1>2)+(3>4)", "1>(2+3+4)", "(1>2)+3+4", "1+2+3+4"
    };
    static constexpr float ratios[] = {0.5f, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    static constexpr const char* ratio_names[] = {"0.5", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"};
    struct Topology { std::array<uint8_t, 4> inputs; uint8_t carriers; };
    static constexpr Topology topologies[] = {
        {{0,1,2,4},8}, {{0,0,3,4},8}, {{0,1,0,6},8}, {{0,0,2,5},8},
        {{0,1,0,4},10}, {{0,1,1,1},14}, {{0,1,0,0},14}, {{0,0,0,0},15}
    };
    static constexpr ParameterPage pages[] = {
        {"FM4",0,4}, {"FM4 Pitch",4,2},
        {"FM4 Op1",6,2}, {"FM4 Op1 ADSR",8,4},
        {"FM4 Op2",12,2}, {"FM4 Op2 ADSR",14,4},
        {"FM4 Op3",18,2}, {"FM4 Op3 ADSR",20,4},
        {"FM4 Op4",24,2}, {"FM4 Op4 ADSR",26,4},
    };
    ModuleParameters controls() { return {"FM4", parameters_, nullptr, nullptr, nullptr, pages}; }
    struct Settings {
        unsigned algorithm;
        float depth, feedback, gain, tuning;
        std::array<float,4> ratios, levels;
        std::array<Adsr::Settings,4> envelopes;
    };
    Settings settings(float cents = 0, const Modulation* d = nullptr,
                      const Modulation* fb = nullptr, const Modulation* gain = nullptr) const
    {
        Settings s{};
        s.algorithm = parameters_[algorithm].get();
        s.depth = effect_parameter(parameters_[depth], d) * 5.12f;
        s.feedback = effect_parameter(parameters_[feedback], fb) * 1.28f;
        s.gain = effect_parameter(parameters_[level], gain) * 0.01f;
        s.tuning = Osc::semitone_ratio(parameters_[pitch].get()) * Osc::cents_ratio(parameters_[fine].get() + cents);
        for (unsigned i=0; i<4; ++i) {
            const auto* op = parameters_ + operators + i * op_parameters;
            s.ratios[i] = ratios[op[ratio].get()]; s.levels[i] = op[op_level].get() * 0.01f;
            s.envelopes[i] = {op[attack].get(), op[decay].get(), op[sustain].get(), op[release].get()};
        }
        return s;
    }
    class Voice {
    public:
        void reset() { for (auto& env : envelopes_) env.reset(); feedback_ = {}; }
        bool active() const
        {
            for (unsigned i=0; i<4; ++i) if ((carriers_ & (1u << i)) && !envelopes_[i].idle()) return true;
            return false;
        }
        void note_on(unsigned note, bool reset_phase, bool retrigger, const Settings& s)
        {
            note_ = note;
            update(s, 1);
            if (reset_phase) {
                phases_ = {}; feedback_ = {}; levels_ = s.levels; level_steps_ = {};
                depth_ = s.depth; depth_step_ = 0; gain_ = s.gain;
            }
            if (retrigger) for (auto& env : envelopes_) env.gate_on();
        }
        void release() { for (auto& env : envelopes_) env.gate_off(); }
        void update(const Settings& s, float glide)
        {
            algorithm_ = s.algorithm; carriers_ = topologies[algorithm_].carriers;
            inputs_ = topologies[algorithm_].inputs;
            inverse_carriers_ = 1.0f / std::popcount(unsigned(carriers_));
            const float base = Osc::note_step(note_) * s.tuning * glide;
            for (unsigned i=0; i<4; ++i) {
                // Frequencies wrap naturally above the sample rate; do not clamp the operator ratios.
                steps_[i] = uint32_t(uint64_t(base * s.ratios[i]));
                envelopes_[i].update_parameters(s.envelopes[i]);
                level_steps_[i] = (s.levels[i] - levels_[i]) / 32;
            }
            depth_step_ = (s.depth - depth_) / 32;
            feedback_amount_ = unsigned(s.feedback);
            target_gain_ = s.gain;
        }
        float next()
        {
            std::array<float,4> value{};
            float output = 0;
            depth_ += depth_step_;
            const unsigned amount = unsigned(std::clamp(depth_,0.f,512.f));
            for (unsigned i=0; i<4; ++i) {
                levels_[i] += level_steps_[i];
                float input = 0;
                for (unsigned j=0; j<i; ++j) if (inputs_[i] & (1u << j)) input += value[j];
                uint32_t offset = Osc::phase_modulation(input, amount);
                if (!i) offset += Osc::phase_modulation((feedback_[0] + feedback_[1]) * 0.5f, feedback_amount_);
                value[i] = Osc::sample(0, phases_[i] + offset) * levels_[i] * envelopes_[i].next();
                phases_[i] += steps_[i];
                if (carriers_ & (1u << i)) output += value[i];
            }
            feedback_ = {feedback_[1],value[0]};
            gain_ += (target_gain_ - gain_) * 0.005f;
            return output * inverse_carriers_ * gain_;
        }
    private:
        std::array<Adsr::Voice,4> envelopes_{Adsr::Voice{44100},Adsr::Voice{44100},Adsr::Voice{44100},Adsr::Voice{44100}};
        std::array<uint32_t,4> phases_{}, steps_{};
        std::array<float,4> levels_{}, level_steps_{};
        std::array<float,2> feedback_{};
        std::array<uint8_t,4> inputs_{};
        unsigned note_=60, algorithm_=4, carriers_=10, feedback_amount_=0;
        float inverse_carriers_=0.5f, gain_=1, target_gain_=1, depth_=0, depth_step_=0;
    };
private:
    Parameter parameters_[30] = {
        {"Algorithm",0,7,1,4,"",algorithms}, {"Depth",0,100,1,30,"%"},
        {"Feedback",0,100,1,0,"%"}, {"Level",0,100,1,100,"%"},
        {"Pitch",-24,24,1,0,"st"}, {"Fine",-100,100,1,0,"ct"},
        {"1 Ratio",0,16,1,1,"",ratio_names}, {"1 Level",0,100,1,65,"%"},
        {"1 Attack",0,2000,5,5,"ms"}, {"1 Decay",0,2000,10,300,"ms"}, {"1 Sustain",0,100,1,60,"%"}, {"1 Release",0,4000,10,180,"ms"},
        {"2 Ratio",0,16,1,1,"",ratio_names}, {"2 Level",0,100,1,90,"%"},
        {"2 Attack",0,2000,5,5,"ms"}, {"2 Decay",0,2000,10,300,"ms"}, {"2 Sustain",0,100,1,80,"%"}, {"2 Release",0,4000,10,180,"ms"},
        {"3 Ratio",0,16,1,2,"",ratio_names}, {"3 Level",0,100,1,50,"%"},
        {"3 Attack",0,2000,5,5,"ms"}, {"3 Decay",0,2000,10,300,"ms"}, {"3 Sustain",0,100,1,60,"%"}, {"3 Release",0,4000,10,180,"ms"},
        {"4 Ratio",0,16,1,1,"",ratio_names}, {"4 Level",0,100,1,90,"%"},
        {"4 Attack",0,2000,5,5,"ms"}, {"4 Decay",0,2000,10,300,"ms"}, {"4 Sustain",0,100,1,80,"%"}, {"4 Release",0,4000,10,180,"ms"},
    };
};
