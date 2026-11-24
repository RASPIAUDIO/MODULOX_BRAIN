#include "filter.h"

#include <cmath>

#include "dsps_biquad.h"
#include "dsps_biquad_gen.h"
#include "esp_attr.h"

ModuleParameters Filter::controls()
{
    return {"Filter", parameters_, [](void* context) {
        static_cast<Filter*>(context)->publish_parameters();
    }, this};
}

Filter::Response Filter::base_response() const
{
    auto settings = topology();
    generate(settings, parameters_[cutoff].get(), resonance_value(nullptr));
    return {settings.mode, settings.stages, settings.coefficients, false};
}

void Filter::publish_response(const Voice* voice)
{
    const auto& settings = tracking_ && voice ? voice->active : active_;
    display_response_.publish({settings.mode, settings.stages, settings.coefficients, voice != nullptr});
}

Filter::Settings Filter::topology() const
{
    Settings next;
    next.mode = parameters_[mode].get();
    next.stages = next.mode == bypass ? 0 : 1 + parameters_[order].get();
    return next;
}

float Filter::resonance_value(const Modulation* modulation) const
{
    const float res = parameters_[resonance].get();
    return (modulation ? modulation->apply(res, 0, 100, 100) : res) * 0.01f;
}

void Filter::generate(Settings& next, float hz, float reso) const
{
    const float frequency = hz / sample_rate_;
    const float q_low = next.stages == 2 && next.mode != band_pass ? 0.5411961f : 0.7071068f;
    const float q_peak = next.stages == 2 && next.mode != band_pass ? 1.306563f : q_low;
    const float q_resonant = q_peak + (8.0f - q_peak) * reso * reso;
    for (int stage = 0; stage < next.stages; ++stage) {
        const float q = stage == next.stages - 1 ? q_resonant : q_low;
        float* coefficients = next.coefficients[stage].data();
        switch (next.mode) {
            case low_pass:  dsps_biquad_gen_lpf_f32(coefficients, frequency, q); break;
            case high_pass: dsps_biquad_gen_hpf_f32(coefficients, frequency, q); break;
            case band_pass: dsps_biquad_gen_bpf0db_f32(coefficients, frequency, q); break;
        }
    }
}

void Filter::publish_parameters()
{
    auto& next = settings_[write_slot_];
    next = topology();
    generate(next, parameters_[cutoff].get(), resonance_value(nullptr));
    write_slot_ = spare_.exchange(write_slot_ | pending, std::memory_order_acq_rel) & ~pending;
}

void Filter::reset_plan()
{
    if (spare_.load(std::memory_order_acquire) & pending) {
        read_slot_ = spare_.exchange(read_slot_, std::memory_order_acq_rel) & ~pending;
    }
    active_ = settings_[read_slot_];
    smoothing_blocks_ = 0;
    modulation_block_ = 0;
    tracking_ = false;
    transition_ = false;
    ++revision_;
}

void IRAM_ATTR Filter::begin_block(const Modulation* cutoff_mod, const Modulation* resonance_mod,
                                  bool voice_cutoff, bool voice_resonance)
{
    transition_ = false;
    mix_ = 0;
    const bool changed = (spare_.load(std::memory_order_acquire) & pending) != 0;
    if (changed) {
        read_slot_ = spare_.exchange(read_slot_, std::memory_order_acq_rel) & ~pending;
        smoothing_blocks_ = 16;
    }
    const Settings* target = &settings_[read_slot_];
    note_tracking_ = cutoff_mod || voice_cutoff;
    tracking_ = note_tracking_ || voice_resonance;
    if (tracking_ || resonance_mod) {
        if (changed || modulation_block_ == 0) {
            modulation_target_ = topology();
            if (modulation_target_.stages) {
                const float hz = parameters_[cutoff].get();
                resonance_target_ = resonance_value(resonance_mod);
                if (tracking_) {
                    const float center = std::log2(hz) + (cutoff_mod ? cutoff_mod->level * cutoff_span_octaves : 0);
                    const float depth = cutoff_mod ? cutoff_mod->depth * cutoff_span_octaves : 0;
                    cutoff_start_octaves_ = center - depth;
                    cutoff_end_octaves_ = center + depth;
                    cutoff_position_ = cutoff_mod ? cutoff_mod->position : 0;
                } else {
                    generate(modulation_target_, hz, resonance_target_);
                }
            }
            ++modulation_revision_;
            smoothing_blocks_ = 4;
        }
        modulation_block_ = (modulation_block_ + 1) % 4;
        target = &modulation_target_;
    }
    if (active_.mode != target->mode || active_.stages != target->stages) {
        previous_ = active_;
        active_ = *target;
        transition_ = true;
        smoothing_blocks_ = 0;
        ++revision_;
    } else if (smoothing_blocks_ > 0) {
        mix_ = 1.0f / smoothing_blocks_--;
        if (!tracking_) {
            interpolate(active_, *target, mix_);
        }
    }
}

void Filter::interpolate(Settings& active, const Settings& target, float mix)
{
    for (int stage = 0; stage < active.stages; ++stage) {
        for (unsigned i = 0; i < active.coefficients[stage].size(); ++i) {
            auto& coefficient = active.coefficients[stage][i];
            coefficient += (target.coefficients[stage][i] - coefficient) * mix;
        }
    }
}

void Filter::update_tracked(Voice& voice, float note, const Modulation* cutoff_mod,
                            const Modulation* resonance_mod) const
{
    const bool new_note = note != voice.note;
    const bool new_topology = voice.revision != revision_;
    if (new_note || new_topology || voice.modulation_revision != modulation_revision_) {
        voice.note = note;
        voice.modulation_revision = modulation_revision_;
        voice.target.mode = active_.mode;
        voice.target.stages = active_.stages;
        if (voice.target.stages) {
            constexpr float min_octaves = 4.321928095f;  // log2(20 Hz)
            constexpr float max_octaves = 14.13570929f; // log2(18 kHz)
            const float key = note_tracking_ ? (note - tracking_reference) * (1.0f / 12) : 0;
            const float level = cutoff_mod ? cutoff_mod->level * cutoff_span_octaves : 0;
            const float depth = cutoff_mod ? cutoff_mod->depth * cutoff_span_octaves : 0;
            const float start = std::clamp(cutoff_start_octaves_ + key + level - depth, min_octaves, max_octaves);
            const float end = std::clamp(cutoff_end_octaves_ + key + level + depth, min_octaves, max_octaves);
            const float position = cutoff_mod ? cutoff_mod->position : cutoff_position_;
            const float hz = std::clamp(std::exp2(start + (end - start) * position),
                                        minimum_hz, maximum_hz);
            generate(voice.target, hz, resonance_mod ? resonance_value(resonance_mod) : resonance_target_);
        }
        // A live note change must retain the current coefficients and history.
        // Only a fresh voice or a topology change can install them immediately.
        if (new_topology) {
            voice.active = voice.target;
            voice.smoothing_blocks = 0;
            return;
        }
        voice.smoothing_blocks = 4;
    }
    if (voice.smoothing_blocks) {
        interpolate(voice.active, voice.target, 1.0f / voice.smoothing_blocks--);
    }
}

void IRAM_ATTR Filter::apply(float* samples, int frames, Settings& settings, History& history)
{
    for (int stage = 0; stage < settings.stages; ++stage) {
        dsps_biquad_f32_aes3(samples, samples, frames, settings.coefficients[stage].data(),
                             history[stage].data());
    }
}

void IRAM_ATTR Filter::process(Voice& voice, float note, float* samples, float* scratch, int frames,
                              const Modulation* cutoff_mod, const Modulation* resonance_mod)
{
    const bool fade = transition_ && voice.revision + 1 == revision_;
    if (fade) {
        std::copy_n(samples, frames + 1, scratch);
        apply(scratch, frames, tracking_ ? voice.active : previous_, voice.history);
    }
    if (tracking_) {
        update_tracked(voice, note, cutoff_mod, resonance_mod);
    }
    if (voice.revision != revision_) {
        voice.revision = revision_;
        voice.history = {};
    }
    apply(samples, frames, tracking_ ? voice.active : active_, voice.history);
    if (fade) {
        const float increment = 1.0f / frames;
        for (int i = 0; i < frames; ++i) {
            const float mix = (i + 1) * increment;
            samples[i] = scratch[i] + mix * (samples[i] - scratch[i]);
        }
    }
}
