#include "osc.h"

#include <cmath>

float Osc::tables_[3][Osc::table_size + 1]{};
uint32_t Osc::note_steps_[128]{};
float Osc::pitch_ratios_[1001]{};
float Osc::semitone_ratios_[2 * pitch_limit + 1]{};

void Osc::prepare(unsigned sample_rate)
{
    constexpr double turn = 6.283185307179586;
    for (unsigned index = 0; index < table_size; ++index) {
        const double position = double(index) / table_size;
        tables_[0][index] = static_cast<float>(std::sin(turn * position));
        tables_[1][index] = index < table_size / 2 ? 1.0f : -1.0f;
        tables_[2][index] = static_cast<float>(1.0 - 4.0 * std::abs(position - 0.5));
    }
    for (auto& table : tables_) {
        table[table_size] = table[0];
    }

    const double phase_per_hertz = 4294967296.0 / sample_rate;
    for (int note = 0; note < 128; ++note) {
        const double frequency = 440.0 * std::exp2((note - 69) / 12.0);
        note_steps_[note] = static_cast<uint32_t>(frequency * phase_per_hertz + 0.5);
    }
    for (int cents = -500; cents <= 500; ++cents) {
        pitch_ratios_[cents + 500] = static_cast<float>(std::exp2(cents / 1200.0));
    }
    for (int semitones = -pitch_limit; semitones <= pitch_limit; ++semitones) {
        semitone_ratios_[semitones + pitch_limit] =
            static_cast<float>(std::exp2(double(semitones) / semitones_per_octave));
    }
}

float Lfo::waveform(int wave, uint32_t phase)
{
    return wave == 1 ? (phase < 0x80000000u ? 1.0f : -1.0f) : Osc::sample(wave, phase);
}

void Osc::load_wave(std::span<const int16_t, wave_samples> samples)
{
    for (unsigned i = 0; i < samples.size(); ++i) {
        custom_[i] = samples[i] * (1.0f / 32768.0f);
    }
    custom_[table_size] = custom_[0];
}

float Osc::cents_ratio(float cents)
{
    const float index = std::clamp(cents, -500.0f, 500.0f) + 500.0f;
    const auto whole = static_cast<unsigned>(index);
    return pitch_ratios_[whole] + (index - whole) *
        (pitch_ratios_[std::min(whole + 1, 1000u)] - pitch_ratios_[whole]);
}

Osc::Settings Osc::settings(float pitch_mod, const Modulation* level_mod, const Modulation* pwm_mod) const
{
    const float ratio = cents_ratio(parameters_[fine].get() + pitch_mod);
    const float base_level = parameters_[level].get();
    const float base_pwm = parameters_[pwm].get();
    const int waveform = parameters_[wave].get();
    Settings result{waveform, waveform < builtin_waves ? tables_[waveform] : custom_,
            ratio * semitone_ratios_[parameters_[pitch].get() + pitch_limit],
            (level_mod ? level_mod->apply(base_level, 0, 100, 100) : base_level) * 0.01f,
            pwm_mod ? pwm_mod->apply(base_pwm, 1, 99, 49) : base_pwm};
    result.copies = parameters_[unison].get();
    result.mix = parameters_[unison_mix].get() * 0.01f;
    if (result.copies > 1) {
        const float spread = parameters_[detune].get();
        for (unsigned i = 0; i < result.copies; ++i)
            result.detune_ratios[i] = cents_ratio(spread * (2.0f * i / (result.copies - 1) - 1));
    }
    return result;
}

void Osc::Voice::note_on(uint8_t note, bool reset_phase, const Settings& settings)
{
    note_ = note;
    update(settings);
    if (reset_phase) {
        phase_ = 0;
        level_ = target_level_;
        pulse_ = target_pulse_;
        for (unsigned i = 0; i < copy_phase_.size(); ++i) copy_phase_[i] = (i + 1) * 0x9e3779b9u;
        mix_ = target_mix_;
    }
}

void Osc::Voice::update(const Settings& settings, float glide_start, float glide_end)
{
    waveform_ = settings.wave;
    samples_ = settings.samples ? settings.samples : Osc::tables_[settings.wave];
    target_level_ = settings.level;
    target_pulse_ = Osc::pulse_width(settings.pwm);
    const float base = note_steps_[note_] * settings.ratio;
    moving_step_ = std::min(2147483520.0f, base * glide_start);
    const float end = std::min(2147483520.0f, base * glide_end);
    step_increment_ = (end - moving_step_) / 32;
    phase_step_ = static_cast<uint32_t>(moving_step_);
    copies_ = settings.copies;
    target_mix_ = settings.mix;
    if (copies_ > 1) {
        inverse_copies_ = 1.0f / copies_;
        detune_ratios_ = settings.detune_ratios;
        for (unsigned i = 0; i < copies_; ++i)
            copy_step_[i] = static_cast<uint32_t>(std::min(2147483520.0f, moving_step_ * detune_ratios_[i]));
    }
}
