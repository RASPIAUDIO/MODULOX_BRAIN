#pragma once

#include "lfo.h"
#include "waves.h"

#include <cstdint>
#include <array>
#include <cmath>

class Osc {
public:
    enum ParameterIndex { wave, pwm, pitch, fine, level, unison, detune, unison_mix, fm };
    static constexpr int pitch_limit = 24;
    static constexpr int semitones_per_octave = 12;

    struct Settings {
        int wave;
        const float* samples;
        float ratio;
        float level;
        float pwm;
        unsigned copies = 1;
        float mix = 1;
        std::array<float, 4> detune_ratios{1, 1, 1, 1};
    };
    ModuleParameters controls(const char* name, bool allow_fm = false)
    {
        static constexpr ParameterPage pages[] = {{"Osc",0,2},{"Osc Pitch",2,3},{"Osc Unison",5,3},{"Osc FM",8,1}};
        return {name, std::span{parameters_}.first(allow_fm ? 9 : 8), nullptr, nullptr,
                allow_fm ? &hidden_parameters_ : nullptr,
                allow_fm ? std::span<const ParameterPage>{pages} : std::span<const ParameterPage>{}};
    }
    void set_fm_source(bool present) { hidden_parameters_.store(present ? 0 : 1u << fm, std::memory_order_relaxed); }
    float fm_amount(const Modulation* mod) const
    {
        const float base = parameters_[fm].get();
        return (mod ? mod->apply(base, 0, 100, 100) : base) * 5.12f;
    }
    static uint32_t note_step(unsigned note) { return note_steps_[note]; }
    static float semitone_ratio(int semitones) { return semitone_ratios_[semitones + pitch_limit]; }
    static uint32_t phase_modulation(float signal, unsigned amount)
    {
        // 24-bit audio times an integer index; unsigned overflow wraps the phase by design.
        return static_cast<uint32_t>(static_cast<int32_t>(signal * 16777216.0f)) * amount;
    }
    static void prepare(unsigned sample_rate);
    static float cents_ratio(float cents);
    void load_wave(std::span<const int16_t, wave_samples> samples);
    float preview_custom(uint32_t phase) const { return sample_table(custom_, phase); }
    Settings settings(float pitch_mod = 0, const Modulation* level_mod = nullptr,
                      const Modulation* pwm_mod = nullptr) const;

    class Voice {
    public:
        void note_on(uint8_t note, bool reset_phase, const Settings& settings);
        void update(const Settings& settings, float glide_start = 1, float glide_end = 1);
        float next() { return next_offset(0); }
        float next_pm(uint32_t offset) { return next_offset(offset); }
        float raw() const { return raw_; }
    private:
        float next_offset(uint32_t offset)
        {
            const uint32_t phase = phase_ + offset;
            phase_ += phase_step_;
            if (step_increment_ != 0) {
                moving_step_ += step_increment_;
                phase_step_ = static_cast<uint32_t>(moving_step_);
            }
            float sample = 0;
            if (waveform_ == square) {
                pulse_ += (target_pulse_ - pulse_) * 0.03125f;
            }
            if (copies_ == 1 || mix_ < 1 || target_mix_ < 1)
                sample = waveform_ == square ? Osc::pulse_sample(phase, static_cast<uint32_t>(pulse_))
                                             : Osc::sample_table(samples_, phase);
            level_ += (target_level_ - level_) * 0.005f;
            if (copies_ > 1) {
                mix_ += (target_mix_ - mix_) * 0.01f;
                if (std::abs(target_mix_ - mix_) < 0.000001f) mix_ = target_mix_;
                float cloud = 0;
                for (unsigned i = 0; i < copies_; ++i) {
                    const auto at = copy_phase_[i] + offset;
                    copy_phase_[i] += copy_step_[i];
                    if (step_increment_ != 0) copy_step_[i] = static_cast<uint32_t>(std::min(2147483520.0f, moving_step_ * detune_ratios_[i]));
                    if (mix_ > 0) cloud += waveform_ == square ? Osc::pulse_sample(at, static_cast<uint32_t>(pulse_))
                                                              : Osc::sample_table(samples_, at);
                }
                sample += (cloud * inverse_copies_ - sample) * mix_;
            }
            raw_ = sample;
            return sample * level_;
        }
    private:
        uint32_t phase_ = 0;
        uint32_t phase_step_ = 0;
        float pulse_ = 2147483648.0f;
        float target_pulse_ = 2147483648.0f;
        uint8_t note_ = 0;
        int waveform_ = 0;
        const float* samples_ = tables_[0];
        float level_ = 1.0f;
        float target_level_ = 1.0f;
        std::array<uint32_t, 4> copy_phase_{}, copy_step_{};
        std::array<float, 4> detune_ratios_{1, 1, 1, 1};
        unsigned copies_ = 1;
        float inverse_copies_ = 1, mix_ = 1, target_mix_ = 1;
        float moving_step_ = 0, step_increment_ = 0;
        float raw_ = 0;
    };

    static float sample(int waveform, uint32_t phase)
    {
        return sample_table(tables_[waveform], phase);
    }

private:
    static float sample_table(const float* samples, uint32_t phase)
    {
        constexpr unsigned fraction_bits = 32 - table_bits;
        const unsigned index = phase >> fraction_bits;
        const float fraction = (phase & ((1u << fraction_bits) - 1)) *
                               (1.0f / (1u << fraction_bits));
        return samples[index] + fraction * (samples[index + 1] - samples[index]);
    }

public:
    static float preview_sample(int waveform, uint32_t phase, float duty)
    {
        return waveform == square ? pulse_sample(phase, static_cast<uint32_t>(pulse_width(duty)))
                                  : sample(waveform, phase);
    }

private:
    static float pulse_width(float duty) { return duty * (4294967296.0f / 100); }
    static float pulse_sample(uint32_t phase, uint32_t width)
    {
        return phase < width ? 1.0f : -1.0f;
    }

    static constexpr unsigned table_bits = 10;
    static constexpr unsigned table_size = wave_samples;
    static_assert(table_size == 1 << table_bits);
    static constexpr int square = 1;
    static constexpr const char* wave_names[] = {"Sine", "Square", "Triangle"};
    Parameter parameters_[9] = {
        {"Wave", 0, builtin_waves + max_patch_waves - 1, 1, 0, "", wave_names},
        {"PWM", 1, 99, 1, 50, "%"},
        {"Pitch", -pitch_limit, pitch_limit, 1, 0, "st"},
        {"Fine", -100, 100, 1, 0, "ct"},
        {"Level", 0, 100, 1, 100, "%"},
        {"Unison", 1, 4, 1, 1},
        {"Detune", 0, 100, 1, 15, "ct"},
        {"Mix", 0, 100, 1, 100, "%"},
        {"FM", 0, 100, 1, 0, "%"},
    };
    std::atomic<unsigned> hidden_parameters_{1u << fm};
    static float tables_[3][table_size + 1];
    static uint32_t note_steps_[128];
    static float pitch_ratios_[1001];
    static float semitone_ratios_[2 * pitch_limit + 1];
    float custom_[table_size + 1]{};
};
