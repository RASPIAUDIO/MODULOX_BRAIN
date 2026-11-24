#include "adsr.h"

void Adsr::Voice::ramp(float target, unsigned milliseconds)
{
    target_ = target;
    samples_left_ = std::max(1u, sample_rate_ * milliseconds / 1000);
    increment_ = (target_ - value_) / samples_left_;
}

void Adsr::Voice::gate_on()
{
    stage_ = Stage::attack;
    ramp(1.0f, settings_[attack]);
}

void Adsr::Voice::gate_off()
{
    stage_ = Stage::release;
    ramp(0.0f, settings_[release]);
}

void Adsr::Voice::advance()
{
    switch (stage_) {
        case Stage::attack:
            stage_ = Stage::decay;
            ramp(settings_[sustain] * 0.01f, settings_[decay]);
            break;
        case Stage::decay:
            stage_ = Stage::sustain;
            break;
        case Stage::release:
            stage_ = Stage::idle;
            break;
        default:
            break;
    }
}

void Adsr::Voice::update_parameters(const Settings& settings)
{
    bool changed[4];
    for (unsigned i = 0; i < 4; ++i) {
        const int value = settings[i];
        changed[i] = settings_[i] != value;
        settings_[i] = value;
    }

    if (stage_ == Stage::attack && changed[attack]) {
        ramp(1.0f, settings_[attack]);
    } else if (stage_ == Stage::decay && (changed[decay] || changed[sustain])) {
        ramp(settings_[sustain] * 0.01f, settings_[decay]);
    } else if (stage_ == Stage::sustain && changed[sustain]) {
        ramp(settings_[sustain] * 0.01f, 5);
    } else if (stage_ == Stage::release && changed[release]) {
        ramp(0.0f, settings_[release]);
    }
}

Adsr::Settings Adsr::settings() const
{
    return {parameters_[attack].get(), parameters_[decay].get(),
            parameters_[sustain].get(), parameters_[release].get()};
}
