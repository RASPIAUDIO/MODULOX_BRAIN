#include "synth_voice.h"

#include <algorithm>
#include <bit>
#include <cmath>

#include "esp_attr.h"

DRAM_ATTR const float SynthVoice::silence_[SynthVoice::block_frames + 1]{};

void SynthVoice::prepare()
{
    Osc::prepare(sample_rate);
    granular_.prepare();
    for (unsigned note = 0; note < note_ratios_.size(); ++note) {
        note_ratios_[note] = std::exp2((int(note) - 60) / 12.0f);
    }
}

ModuleParameters SynthVoice::oscillator_controls(unsigned index)
{
    constexpr const char* labels[] = {"Osc", "Osc 2", "Osc 3"};
    return oscillators_[index].controls(labels[index], index == 0);
}

void SynthVoice::reset(Voice& voice)
{
    voice.held = false;
    voice.pitch.reset();
    voice.noise_level = voice.vca_gain = 0;
    voice.folder = {};
    voice.pans = {};
    voice.fm.reset();
    voice.osc_fm_amount = voice.osc_fm_step = voice.osc_fm_target = 0;
    voice.osc_fm = false;
    for (unsigned i = 0; i < plan_.samplers; ++i) voice.samplers[i].reset();
    if (plan_.has(granulizer)) voice.grains.reset();
    for (unsigned i = 0; i < utility_slots.size(); ++i) {
        if (plan_.has(utility_slots[i])) utilities_[i].reset(voice.controls[i], utility_kind(utility_slots[i]));
    }
    voice.envelope.reset();
    for (auto& env : voice.mod_envelopes) env.reset();
    voice.filter = {};
}

void SynthVoice::select_plan(const AudioPlan& plan)
{
    plan_ = plan;
    oscillators_[0].set_fm_source(plan.has(oscillator2));
    configure_audio_plan(plan_);
    reset(post_voice_);
    monophonic_ = settings_.parameters[play_mode].get();
    held_notes_.reset();
    arp_.set_external_trigger(plan.controls[arp][control_trigger].source != no_module);
    grain_envelope_modulation_ = false;
    for (unsigned port = grain_start; port <= grain_level; ++port) {
        grain_envelope_modulation_ |= (plan.modulation & (1ull << port)) && plan.per_voice_source(plan.sources[port]);
    }
    sample_preview_.reset();
    lfo_phase_.fill(0);
    lfo_sync_offsets_.fill(0);
    arp_.reset();
    arp_enabled_ = plan_.has(arp) && arp_.enabled();
    midi_trigger_ = arp_trigger_ = false;
    for (unsigned i = 0; i < utility_slots.size(); ++i) {
        utilities_[i].reset(control_states_[i], utility_kind(utility_slots[i]));
        control_states_[i].random ^= (i + 1) * 0x9e3779b9u;
        control_values_[i] = {0.5f, 1, 0};
    }
    last_note_ = 60;
    last_velocity_ = 1;
    delay_.reset();
    chorus_.reset();
    rotary_.reset();
    output_gain_ = 1.0f;
    for (auto& voice : voices_) {
        reset(voice);
    }
    filter_.reset_plan();
}

void SynthVoice::configure_transport()
{
    transport_.configure({unsigned(settings_.parameters[tempo_bpm].get()),
                          unsigned(settings_.parameters[clock_source].get()),
                          unsigned(settings_.parameters[transport_run].get()),
                          unsigned(settings_.parameters[sync_ppqn].get()),
                          unsigned(settings_.parameters[clock_loss].get())});
}

void SynthVoice::receive_clock(const ClockEvent& event)
{
    configure_transport();
    transport_.receive(event);
}

void SynthVoice::midi_note_on(uint8_t source, uint8_t channel, uint8_t note, uint8_t velocity)
{
    update_arp_mode();
    const int selected = settings_.parameters[midi_channel].get();
    if (selected && channel + 1 != selected) return;
    midi_trigger_ = true;
    if (plan_.has(arp) && arp_.enabled()) arp_.note_on({source, channel, note, velocity});
    else note_on(source, channel, note, velocity);
}

void SynthVoice::midi_note_off(uint8_t source, uint8_t channel, uint8_t note)
{
    update_arp_mode();
    if (plan_.has(arp)) arp_.note_off({source, channel, note, 0});
    if (!plan_.has(arp) || !arp_.enabled()) note_off(source, channel, note);
}

void SynthVoice::midi_clear(uint8_t source, int channel, bool immediate)
{
    if (plan_.has(arp)) arp_.clear_source(source, channel);
    all_notes_off(source, channel, immediate);
    if (channel < 0) transport_.disconnect(source + 1);
}

bool SynthVoice::event_input(int slot, unsigned port, bool fallback) const
{
    const auto link = plan_.controls[slot][port];
    if (link.source == no_module) return fallback;
    if (clock_module(link.source)) return clock_outputs_[link.source - clock1][link.output];
    return link.source == midi ? midi_trigger_ : link.source == arp ? arp_trigger_ : false;
}

void SynthVoice::update_timing()
{
    update_arp_mode();
    configure_transport();
    transport_.advance(block_frames, sample_rate, now_);
    for (unsigned i = 0; i < 2; ++i) {
        if (!plan_.has(clock1 + i)) continue;
        clock_outputs_[i] = {transport_.pulse(clocks_[i].get(0)), transport_.reset(), transport_.running()};
    }
    arp_trigger_ = false;
    if (plan_.has(arp)) {
        arp_.process(transport_, event_input(arp, control_trigger, transport_.pulse(arp_.division())),
            event_input(arp, control_reset, transport_.reset()), event_input(arp, control_run, transport_.running()),
            [this](Arpeggiator::Note note) {
                arp_trigger_ = true;
                note_on(note.source, note.channel, note.pitch, note.velocity);
            }, [this](Arpeggiator::Note note) { note_off(note.source, note.channel, note.pitch); });
    }
}

void SynthVoice::update_arp_mode()
{
    const bool enabled = plan_.has(arp) && arp_.enabled();
    if (arp_enabled_ == enabled) return;
    all_notes_off(0, -1, true);
    all_notes_off(1, -1, true);
    arp_.reset();
    arp_enabled_ = enabled;
}

void SynthVoice::update_controls(Voice* voice)
{
    for (unsigned step = 0; step < plan_.control_count; ++step) {
        const int slot = plan_.control_order[step];
        const unsigned i = slot - sample_hold;
        if (plan_.voice_controls[i] != bool(voice)) continue;
        auto& state = voice ? voice->controls[i] : control_states_[i];
        if (event_input(slot, control_reset, transport_.reset())) {
            utilities_[i].reset(state, utility_kind(slot));
            state.random ^= (i + 1) * 0x9e3779b9u;
        }
        const int source = plan_.controls[slot][cv_value].source;
        const Modulation* input = source_modulation(source, voice ? &env_values_ : nullptr);
        const float value = input ? input->apply(0, -1, 1, 1) : 0;
        const bool trigger = event_input(slot, control_trigger);
        const float out = utilities_[i].process(state, utility_kind(slot), value, trigger, block_frames, sample_rate);
        auto& output = voice ? voice_control_values_[i] : control_values_[i];
        output = {0.5f + out * 0.5f, 1, 0};
    }
}

void SynthVoice::play_sample(unsigned index)
{
    if (index > sample_slots.size() || (index < sample_slots.size() && !plan_.has(sample_slots[index]))) return;
    const auto& source = index < samplers_.size() ? samplers_[index] :
        index == sample_slots.size() ? library_preview_ : granular_preview_;
    if (source.samples().size() < 2) return;
    preview_index_ = index;
    sample_preview_.note_on(source, true);
}

bool SynthVoice::active(const Voice& voice) const
{
    bool vca_release = false;
    if (plan_.has(vca)) for (unsigned i = 0; i < env_slots.size(); ++i)
        vca_release |= plan_.has(env_slots[i]) && !voice.mod_envelopes[i].idle();
    const bool fm_active = plan_.has(fm4) && voice.fm.active();
    if (plan_.has_envelope) return !voice.envelope.idle() || vca_release || fm_active;
    if (fm_active) return true;
    if ((plan_.oscillators || plan_.has(noise) || plan_.has(sub)) && voice.held) return true;
    // A CV envelope driving a VCA must finish its release even without an audio ADSR.
    if (vca_release) return true;
    if (plan_.has(granulizer) && (voice.held || voice.grains.active())) return true;
    for (unsigned i = 0; i < plan_.samplers; ++i) {
        if (voice.samplers[i].active()) return true;
    }
    return false;
}

void SynthVoice::update_limits()
{
    const bool mono = settings_.parameters[play_mode].get();
    if (mono != monophonic_) {
        for (auto& voice : voices_) reset(voice);
        held_notes_.reset();
        monophonic_ = mono;
    }
    const unsigned limit = monophonic_ ? 1 : settings_.parameters[polyphony].get();
    const int channel = settings_.parameters[midi_channel].get();
    if (channel != midi_channel_) {
        midi_channel_ = channel;
        arp_.filter_channel(channel);
        held_notes_.filter_channel(channel);
        for (unsigned i = 0; i < voice_limit_; ++i) {
            if (channel && voices_[i].channel + 1 != channel) {
                reset(voices_[i]);
            }
        }
    }
    for (unsigned i = limit; i < voice_limit_; ++i) {
        reset(voices_[i]);
    }
    voice_limit_ = limit;
}

void SynthVoice::note_on(uint8_t source, uint8_t channel, uint8_t note, uint8_t velocity)
{
    update_limits();
    if (midi_channel_ && channel + 1 != midi_channel_) {
        return;
    }
    unsigned triggered_samples = 0;
    for (unsigned i = 0; i < plan_.samplers; ++i) {
        if (samplers_[i].accepts(note, channel)) triggered_samples |= 1u << i;
    }
    const bool grains = plan_.has(granulizer) && granular_.ready();
    if (!plan_.oscillators && !triggered_samples && !grains && !plan_.has(noise) && !plan_.has(sub) && !plan_.has(fm4)) return;

    const bool overlap = !held_notes_.empty();
    held_notes_.on({source, channel, note, velocity});

    last_note_ = note;
    last_velocity_ = velocity * (1.0f / 127);
    for (unsigned i = 0; i < lfo_slots.size(); ++i) {
        if (!plan_.explicit_midi() || plan_.midi_to(lfo_slots[i], midi_trigger)) {
            lfo_phase_[i] = 0;
            lfo_sync_offsets_[i] = transport_.beat();
        }
    }
    if (plan_.modulation & ((1ull << cutoff_port) | (1ull << resonance_port))) {
        filter_.restart_modulation();
    }

    // Retrigger matching notes. Otherwise prefer a free voice, then the oldest release.
    Voice* selected = &voices_[0];
    int priority = -1;
    const uint32_t age = ++clock_;
    for (unsigned i = 0; i < voice_limit_; ++i) {
        auto& voice = voices_[i];
        if (active(voice) && voice.source == source && voice.channel == channel && voice.note == note) {
            selected = &voice;
            break;
        }
        const int candidate = !active(voice) ? 2 : !voice.held ? 1 : 0;
        if (candidate > priority || (candidate == priority && age - voice.age > age - selected->age)) {
            selected = &voice;
            priority = candidate;
        }
    }
    auto& voice = *selected;
    const bool idle = !active(voice);
    const bool stolen = !idle && voice_limit_ > 1 &&
                        (voice.source != source || voice.channel != channel || voice.note != note);
    const bool repeated = voice.held && voice.source == source &&
                          voice.channel == channel && voice.note == note;
    if (stolen) {
        const auto previous_pitch = voice.pitch;
        reset(voice);
        voice.pitch = previous_pitch;
    }
    voice.source = source;
    voice.channel = channel;
    voice.note = note;
    voice.age = age;
    voice.held = true;
    voice.velocity = last_velocity_;
    voice.target_gain = 0.25f * (plan_.explicit_midi() ? 1.0f : voice.velocity);
    if (idle || stolen) voice.gain = voice.target_gain;
    const int glide = settings_.parameters[glide_mode].get();
    voice.pitch.target(note, glide == 1 || (glide == 2 && overlap),
                       settings_.parameters[glide_time].get(), sample_rate);
    const bool retrigger = !monophonic_ || !overlap || settings_.parameters[legato_retrigger].get();
    voice.noise = age * 0x9e3779b9u ^ (note + 1u);
    if (!voice.noise) voice.noise = 1;
    if (grains && retrigger) voice.grains.trigger(age * 0x9e3779b9u ^ note);
    for (unsigned i = 0; i < plan_.samplers; ++i) {
        if (retrigger) {
            voice.samplers[i].reset();
            if (triggered_samples & (1u << i)) voice.samplers[i].note_on(samplers_[i]);
        }
    }
    for (unsigned i = 0; i < plan_.oscillators; ++i) {
        const int slot = oscillator_slots[i];
        const auto key = !plan_.explicit_midi() || plan_.midi_to(slot, midi_pitch) ? note : 60;
        auto settings = oscillators_[i].settings();
        if (plan_.midi_to(slot, midi_velocity)) settings.level *= voice.velocity;
        voice.oscillators[i].note_on(key, idle || stolen || (retrigger && plan_.midi_to(slot, midi_trigger)),
                                     settings);
    }
    if (plan_.has(sub)) {
        auto settings = sub_.settings();
        if (plan_.midi_to(sub, midi_velocity)) settings.level *= voice.velocity;
        voice.sub.note_on(!plan_.explicit_midi() || plan_.midi_to(sub, midi_pitch) ? note : 60,
                         idle || stolen || (retrigger && plan_.midi_to(sub, midi_trigger)), settings);
    }
    if (plan_.has(fm4)) {
        const unsigned key = !plan_.explicit_midi() || plan_.midi_to(fm4, midi_pitch) ? note : 60;
        voice.fm.note_on(key, idle || stolen || (retrigger && plan_.midi_to(fm4, midi_trigger)),
                         retrigger || idle, fm4_.settings());
    }
    if (idle) {
        voice.filter = {};
    }
    if (retrigger || idle) gate_envelopes(voice, true, repeated);
}

void SynthVoice::release(Voice& voice)
{
    if (!voice.held) {
        return;
    }
    voice.held = false;
    for (unsigned i = 0; i < plan_.samplers; ++i) voice.samplers[i].release();
    if (plan_.has(fm4)) voice.fm.release();
    gate_envelopes(voice, false);
}

void SynthVoice::gate_envelopes(Voice& voice, bool on, bool repeated)
{
    const auto gate = [on](Adsr::Voice& env, const Adsr& module) {
        env.update_parameters(module.settings());
        if (on) env.gate_on();
        else env.gate_off();
    };
    if (plan_.has_envelope) gate(voice.envelope, envelope_);
    for (unsigned i = 0; i < env_slots.size(); ++i) {
        const int slot = env_slots[i];
        const bool trigger = !plan_.explicit_midi() || plan_.midi_to(slot, midi_trigger);
        const bool gate_connected = plan_.midi_to(slot, midi_gate);
        if (plan_.has(slot) && (trigger || (gate_connected && !repeated))) {
            gate(voice.mod_envelopes[i], envs_[i]);
        }
    }
}

void SynthVoice::note_off(uint8_t source, uint8_t channel, uint8_t note)
{
    held_notes_.off(source, channel, note);
    for (unsigned i = 0; i < voice_limit_; ++i) {
        auto& voice = voices_[i];
        if (voice.source == source && voice.channel == channel && voice.note == note) {
            if (monophonic_ && !held_notes_.empty()) {
                const auto previous = held_notes_.last();
                note_on(previous.source, previous.channel, previous.pitch, previous.velocity);
                return;
            }
            release(voice);
        }
    }
}

void SynthVoice::all_notes_off(uint8_t source, int channel, bool immediate)
{
    held_notes_.clear(source, channel);
    for (unsigned i = 0; i < voice_limit_; ++i) {
        auto& voice = voices_[i];
        if (voice.source == source && (channel < 0 || voice.channel == channel)) {
            if (immediate) {
                reset(voice);
            } else {
                release(voice);
            }
        }
    }
    if (monophonic_ && !voices_[0].held && !held_notes_.empty()) {
        const auto previous = held_notes_.last();
        note_on(previous.source, previous.channel, previous.pitch, previous.velocity);
    }
}

void SynthVoice::update_lfos()
{
    constexpr float phase_per_rate = 4294967296.0 / 20.0 * block_frames / sample_rate;
    for (unsigned i = 0; i < lfo_slots.size(); ++i) {
        const int slot = lfo_slots[i];
        if (!plan_.has(slot)) continue;
        const auto settings = lfos_[i].settings();
        if (event_input(slot, control_reset, transport_.reset())) {
            lfo_phase_[i] = 0;
            lfo_sync_offsets_[i] = transport_.beat();
        }
        const bool run = event_input(slot, control_run, lfos_[i].synced() ? transport_.running() : true);
        if (lfos_[i].synced() && run) {
            const double phase = (transport_.beat() - lfo_sync_offsets_[i]) / division_beats[lfos_[i].division()]
                + lfos_[i].phase() / 360.0;
            lfo_phase_[i] = static_cast<uint32_t>((phase - std::floor(phase)) * 4294967296.0);
        }
        const uint32_t phase = lfos_[i].synced() ? lfo_phase_[i] : lfo_phase_[i] + uint32_t(lfos_[i].phase() * (4294967296.0 / 360));
        const float position = 0.5f + 0.5f * Lfo::waveform(settings.wave, phase);
        const float ratio = plan_.midi_to(slot, midi_pitch) ? note_ratios_[last_note_] : 1;
        const float velocity = plan_.midi_to(slot, midi_velocity) ? last_velocity_ : 1;
        if (!lfos_[i].synced() && run) lfo_phase_[i] += static_cast<uint32_t>(std::min(2147483520.0f, settings.rate * phase_per_rate * ratio));
        lfo_values_[i] = {position, settings.depth * 0.01f * velocity, settings.level * 0.01f};
    }
}

const Modulation* SynthVoice::modulation(unsigned port, const std::array<Modulation, 2>* env) const
{
    if (!(plan_.modulation & (1ull << port))) return nullptr;
    return source_modulation(plan_.sources[port], env);
}

const Modulation* SynthVoice::source_modulation(int slot, const std::array<Modulation, 2>* env) const
{
    if (!modulation_source(slot)) return nullptr;
    if (envelope_source(slot)) return env ? &(*env)[slot - env1] : nullptr;
    if (utility_source(slot)) {
        if (plan_.per_voice_source(slot)) return env ? &voice_control_values_[slot - sample_hold] : nullptr;
        return &control_values_[slot - sample_hold];
    }
    return &lfo_values_[slot - lfo];
}

Granulizer::Settings SynthVoice::grain_settings(const std::array<Modulation, 2>* envelopes) const
{
    Granulizer::Modulations inputs{};
    for (unsigned i = 0; i < inputs.size(); ++i) inputs[i] = modulation(grain_start + i, envelopes);
    return granular_.settings(inputs);
}

void SynthVoice::update_oscillators(std::array<Osc::Settings, 3>& osc,
                                   const std::array<Modulation, 2>* env) const
{
    const auto pitch_at = [&](unsigned port) {
        const auto* input = modulation(port, env);
        return input ? input->apply(0, -200, 200, 200) : 0;
    };
    const float pitch = pitch_at(global_pitch);
    for (unsigned i = 0; i < plan_.oscillators; ++i) {
        const unsigned port = osc_pitch + 3 * i;
        osc[i] = oscillators_[i].settings(pitch + pitch_at(port), modulation(port + 1, env),
                                         modulation(port + 2, env));
    }
}

void IRAM_ATTR SynthVoice::render(int16_t* stereo)
{
    update_timing();
    update_limits();
    unsigned sounding = 0;
    for (unsigned i = 0; i < voice_limit_; ++i) {
        if (active(voices_[i])) {
            sounding |= 1u << i;
        }
    }

    if (plan_.modulation || plan_.control_count) update_lfos();
    update_controls();
    const bool has_delay = plan_.has(delay);
    const bool tails = has_delay || plan_.has(chorus) || plan_.has(rotary);
    if (!sounding && !tails && !sample_preview_.active()) {
        midi_trigger_ = false;
        std::fill_n(stereo, 2 * block_frames, int16_t{0});
        return;
    }

    update_oscillators(osc_block_, nullptr);
    if (plan_.has(fm4)) {
        const auto* global = modulation(global_pitch);
        const auto* pitch = modulation(fm4_pitch);
        const float cents = (global ? global->apply(0,-200,200,200) : 0) +
                            (pitch ? pitch->apply(0,-200,200,200) : 0);
        fm_block_ = fm4_.settings(cents,modulation(fm4_depth),modulation(fm4_feedback),modulation(fm4_level));
    }
    if (plan_.has(granulizer) && !grain_envelope_modulation_) grain_block_ = grain_settings();
    for (unsigned i = 0; i < plan_.samplers; ++i) sampler_block_[i] = samplers_[i].settings();
    if (plan_.has_envelope) {
        envelope_block_ = envelope_.settings();
    }
    if (plan_.modules & (1u << mixer)) {
        mixer_.update_parameters();
    }
    if (plan_.has(stereo_mix)) stereo_mixer_.update_parameters();
    if (plan_.has_filter) {
        filter_.begin_block(modulation(cutoff_port), modulation(resonance_port),
                            plan_.per_voice_source(plan_.sources[cutoff_port]), plan_.per_voice_source(plan_.sources[resonance_port]));
    }
    if (plan_.has(disto)) distortion_.begin_block(modulation(disto_mode), modulation(disto_drive), modulation(disto_mix));

    constexpr float gain = 32767.0f;
    for (unsigned i = 0; i < plan_.mix_count; ++i) {
        mix_buses_[i].left.fill(0); mix_buses_[i].right.fill(0);
    }
    if (sounding) {
        render_voice<false>(voices_[std::countr_zero(sounding)], gain);
        sounding &= sounding - 1;
    } else {
        std::fill_n(sum_, block_frames, 0.0f);
    }
    while (sounding) {
        render_voice<true>(voices_[std::countr_zero(sounding)], gain);
        sounding &= sounding - 1;
    }
    const auto write = [stereo](unsigned frame, float left, float right) {
        constexpr float threshold = 0.75f * 32767.0f;
        constexpr float headroom = 32767.0f - threshold;
        const float magnitude = std::max(std::abs(left), std::abs(right));
        if (magnitude > threshold) {
            const float excess = magnitude - threshold;
            const float factor = (threshold + headroom * (excess / (headroom + excess))) / magnitude;
            left *= factor; right *= factor;
        }
        stereo[2 * frame] = static_cast<int16_t>(std::clamp(left, -32768.0f, 32767.0f));
        stereo[2 * frame + 1] = static_cast<int16_t>(std::clamp(right, -32768.0f, 32767.0f));
    };
    if (plan_.mix_graph) render_shared();
    else {
        if (has_delay) delay_.process(sum_, block_frames, modulation(delay_time),
                                      modulation(delay_feedback), modulation(delay_mix), transport_.bpm());
        std::copy_n(sum_, block_frames, sum_right_);
    }
    if (sample_preview_.active()) {
        const auto& source = preview_index_ < samplers_.size() ? samplers_[preview_index_] :
            preview_index_ == sample_slots.size() ? library_preview_ : granular_preview_;
        const auto settings = source.settings();
        sample_preview_.render(scratch_, block_frames, source.samples(),
                               settings, settings.root_note, note_ratios_, 0);
        for (unsigned frame = 0; frame < block_frames; ++frame) {
            const float sample = scratch_[frame] * (gain * 0.5f);
            sum_[frame] += sample; sum_right_[frame] += sample;
        }
    }
    if (const auto* level = modulation(output_level)) {
        const float target = level->apply(1.0f, 0.0f, 1.0f, 1.0f);
        const float increment = (target - output_gain_) / block_frames;
        for (unsigned frame = 0; frame < block_frames; ++frame) {
            output_gain_ += increment;
            write(frame, sum_[frame] * output_gain_, sum_right_[frame] * output_gain_);
        }
        output_gain_ = target;
    } else {
        for (unsigned frame = 0; frame < block_frames; ++frame) {
            write(frame, sum_[frame], sum_right_[frame]);
        }
    }
    midi_trigger_ = false;
}

void SynthVoice::publish_filter_response()
{
    const Voice* selected = nullptr;
    if (plan_.has_filter) {
        for (unsigned i = 0; i < voice_limit_; ++i)
            if (active(voices_[i]) && (!selected || voices_[i].age > selected->age)) selected = &voices_[i];
        if (selected && plan_.mix_graph && plan_.shared[filter]) selected = &post_voice_;
    }
    filter_.publish_response(selected ? &selected->filter : nullptr);
}

template<bool add>
void SynthVoice::render_voice(Voice& voice, float gain)
{
    const float start_note = voice.pitch.value();
    const float end_note = voice.pitch.advance(block_frames);
    voice.glide_start = start_note == voice.note ? 1 : note_ratio(start_note) / note_ratios_[voice.note];
    voice.glide_end = end_note == voice.note ? 1 : note_ratio(end_note) / note_ratios_[voice.note];
    auto osc = osc_block_;
    if (plan_.has(env1) || plan_.has(env2)) {
        for (unsigned i = 0; i < env_slots.size(); ++i) {
            if (!plan_.has(env_slots[i])) continue;
            auto& env = voice.mod_envelopes[i];
            env.update_parameters(envs_[i].settings());
            env_values_[i] = {1, 0, env.next() * envs_[i].depth()};
        }
        update_controls(&voice);
        update_oscillators(osc, &env_values_);
        if (plan_.per_voice_source(plan_.sources[output_level])) {
            gain *= modulation(output_level, &env_values_)->apply(1, 0, 1, 1);
        }
    }
    for (unsigned i = 0; i < plan_.oscillators; ++i) {
        if (plan_.midi_to(oscillator_slots[i], midi_velocity)) osc[i].level *= voice.velocity;
        const bool follow = !plan_.explicit_midi() || plan_.midi_to(oscillator_slots[i], midi_pitch);
        voice.oscillators[i].update(osc[i], follow ? voice.glide_start : 1, follow ? voice.glide_end : 1);
    }
    if (plan_.has(sub)) {
        const auto* cv = modulation(sub_pitch, &env_values_);
        auto settings = sub_.settings(cv ? cv->apply(0, -200, 200, 200) : 0, modulation(sub_level, &env_values_));
        if (plan_.midi_to(sub, midi_velocity)) settings.level *= voice.velocity;
        const bool follow = !plan_.explicit_midi() || plan_.midi_to(sub, midi_pitch);
        voice.sub.update(settings, follow ? voice.glide_start : 1, follow ? voice.glide_end : 1);
    }
    if (plan_.has(fm4)) {
        auto settings = fm_block_;
        bool per_voice = plan_.per_voice_source(plan_.sources[global_pitch]);
        for (unsigned port = fm4_pitch; port <= fm4_level; ++port)
            per_voice |= plan_.per_voice_source(plan_.sources[port]);
        if (per_voice) {
            const auto* global = modulation(global_pitch,&env_values_);
            const auto* pitch = modulation(fm4_pitch,&env_values_);
            const float cents = (global ? global->apply(0,-200,200,200) : 0) +
                                (pitch ? pitch->apply(0,-200,200,200) : 0);
            settings = fm4_.settings(cents,modulation(fm4_depth,&env_values_),
                modulation(fm4_feedback,&env_values_),modulation(fm4_level,&env_values_));
        }
        if (plan_.midi_to(fm4,midi_velocity)) settings.gain *= voice.velocity;
        voice.fm.update(settings, !plan_.explicit_midi() || plan_.midi_to(fm4,midi_pitch) ? voice.glide_end : 1);
    }
    voice.osc_fm_target = plan_.has(oscillator2) ? oscillators_[0].fm_amount(modulation(osc_fm,&env_values_)) : 0;
    voice.osc_fm = voice.osc_fm_target > 0 || voice.osc_fm_amount > 0.0001f;
    voice.osc_fm_step = (voice.osc_fm_target - voice.osc_fm_amount) / block_frames;
    if (plan_.has_envelope) {
        voice.envelope.update_parameters(envelope_block_);
    }
    // Preserve amplitude across mono note/velocity changes, including glide.
    for (auto& value : voice_gains_) {
        voice.gain += (voice.target_gain - voice.gain) * 0.005f;
        value = voice.gain;
    }
    switch (plan_.path) {
        case AudioPlan::Path::basic: render_active<false, add>(voice, gain); break;
        case AudioPlan::Path::triple: render_active<true, add>(voice, gain); break;
        case AudioPlan::Path::graph: render_graph<add>(voice, gain); break;
    }
    voice.osc_fm_amount = voice.osc_fm_target;
}

float SynthVoice::note_ratio(float note) const
{
    const float at = std::clamp(note, 0.0f, 127.0f);
    const auto whole = unsigned(at);
    return note_ratios_[whole] + (at - whole) * (note_ratios_[std::min(whole + 1, 127u)] - note_ratios_[whole]);
}

void SynthVoice::filter_voice(Voice& voice, float* samples)
{
    const auto per_voice = [&](unsigned port) {
        return plan_.per_voice_source(plan_.sources[port]) ? modulation(port, &env_values_) : nullptr;
    };
    const float note = &voice == &post_voice_ ? voice.note : voice.pitch.value();
    filter_.process(voice.filter, note, samples, scratch_, block_frames,
                    per_voice(cutoff_port), per_voice(resonance_port));
}

template<bool add>
void SynthVoice::accumulate(const float* signal, float gain)
{
    for (unsigned frame = 0; frame < block_frames; ++frame) {
        if constexpr (add) {
            sum_[frame] += signal[frame] * gain * voice_gains_[frame];
        } else {
            sum_[frame] = signal[frame] * gain * voice_gains_[frame];
        }
    }
}

template<bool add>
void SynthVoice::render_graph(Voice& voice, float gain)
{
    const auto signal = [this](int8_t slot) {
        return slot == no_module ? silence_ : signals_[slot].data();
    };
    // Delay is the last node when present. It runs once on the summed voices.
    const unsigned steps = plan_.count;
    const int8_t output = plan_.output;
    for (unsigned step = 0; step < steps; ++step) {
        const int8_t slot = plan_.order[step];
        if (plan_.shared[slot]) continue;
        float* samples = signals_[slot].data();
        const auto& inputs = plan_.inputs[slot];
        const float* first = signal(inputs[0]);
        if (slot >= pan && slot < fm4) {
            const float* first_right = inputs[0] < 0 ? silence_ : right_signal(inputs[0]);
            if (render_stereo_node(slot, voice, samples, right_signal(slot), first, first_right, false)) continue;
        }
        switch (slot) {
        case fm4:
            for (unsigned frame = 0; frame < block_frames; ++frame) samples[frame] = voice.fm.next();
            break;
        case granulizer: {
            auto settings = grain_envelope_modulation_ ? grain_settings(&env_values_) : grain_block_;
            settings.ratio *= note_ratios_[voice.note];
            settings.ratio *= voice.glide_end;
            if (plan_.explicit_midi()) settings.gain *= voice.velocity;
            voice.grains.render(granular_, settings, voice.held, samples, block_frames);
            break;
        }
        case sampler:
        case sampler2:
        case sampler3: {
            const unsigned index = slot - sampler;
            const Modulation* input = modulation(global_pitch, &env_values_);
            const float pitch = (input ? input->apply(0, -200, 200, 200) : 0) +
                (voice.pitch.value() - voice.note) * 100;
            auto settings = sampler_block_[index];
            settings.gain *= 2.0f;
            if (plan_.explicit_midi()) settings.gain *= voice.velocity;
            voice.samplers[index].render(samples, block_frames, samplers_[index].samples(), settings,
                                         voice.note, note_ratios_, pitch);
            break;
        }
        case oscillator:
        case oscillator2:
        case oscillator3: {
            auto& osc = voice.oscillators[slot == oscillator ? 0 : slot - oscillator2 + 1];
            if (slot == oscillator && voice.osc_fm) {
                for (unsigned frame = 0; frame < block_frames; ++frame)
                    samples[frame] = osc.next_pm(oscillator_fm(voice,fm_modulator_[frame]));
            } else for (unsigned frame = 0; frame < block_frames; ++frame) {
                samples[frame] = osc.next();
                if (slot == oscillator2) fm_modulator_[frame] = osc.raw();
            }
            break;
        }
        case mixer: {
            const float* second = signal(inputs[1]);
            const float* third = signal(inputs[2]);
            for (unsigned frame = 0; frame < block_frames; ++frame) {
                samples[frame] = mixer_.sum(first[frame], second[frame], third[frame]);
            }
            break;
        }
        case noise: {
            const float target = noise_.level(modulation(noise_level, &env_values_)) *
                (plan_.midi_to(noise, midi_velocity) ? voice.velocity : 1);
            const float step = (target - voice.noise_level) / block_frames;
            for (unsigned frame = 0; frame < block_frames; ++frame) {
                voice.noise_level += step;
                samples[frame] = Noise::next(voice.noise) * voice.noise_level;
            }
            voice.noise_level = target;
            break;
        }
        case sub:
            for (unsigned frame = 0; frame < block_frames; ++frame) samples[frame] = voice.sub.next();
            break;
        case vca: {
            const float target = vca_.gain(modulation(vca_gain, &env_values_),
                plan_.midi_to(vca, midi_velocity) ? voice.velocity : 1);
            const float step = (target - voice.vca_gain) / block_frames;
            for (unsigned frame = 0; frame < block_frames; ++frame) {
                voice.vca_gain += step;
                samples[frame] = first[frame] * voice.vca_gain;
            }
            voice.vca_gain = target;
            break;
        }
        case ring_mod: {
            const float mix = ring_.mix(modulation(ring_mix, &env_values_));
            const float* second = signal(inputs[1]);
            for (unsigned frame = 0; frame < block_frames; ++frame)
                samples[frame] = first[frame] + mix * (first[frame] * second[frame] - first[frame]);
            break;
        }
        case wavefolder: {
            const auto settings = folder_.settings(modulation(fold_drive, &env_values_),
                modulation(fold_symmetry, &env_values_), modulation(fold_mix, &env_values_));
            for (unsigned frame = 0; frame < block_frames; ++frame)
                samples[frame] = Wavefolder::process(first[frame], settings, voice.folder);
            break;
        }
        case filter:
            std::copy_n(first, block_frames + 1, samples);
            if (filter_.enabled()) {
                filter_voice(voice, samples);
            }
            break;
        case envelope:
            for (unsigned frame = 0; frame < block_frames; ++frame) {
                samples[frame] = first[frame] * voice.envelope.next();
            }
            break;
        case disto:
            for (unsigned frame = 0; frame < block_frames; ++frame) {
                samples[frame] = distortion_.process(first[frame]);
            }
            break;
        default:
            break;
        }
    }
    if (plan_.mix_graph) collect_voice(gain, add);
    else accumulate<add>(signal(output), gain);
}

float* SynthVoice::right_signal(int slot)
{
    // Legacy Delay was terminal: its new modes can safely expose R without changing old mono nodes.
    if (slot == delay) return right_signals_[0].data();
    if (!plan_.stereo_graph) return signals_[slot].data();
    const int at = stereo_buffer(slot);
    return plan_.stereo_graph && at >= 0 ? right_signals_[at].data() : signals_[slot].data();
}

void SynthVoice::collect_voice(float gain, bool add)
{
    for (unsigned bus = 0; bus < plan_.mix_count; ++bus) {
        const int slot = plan_.mix_slots[bus];
        const float* left = signals_[slot].data();
        const float* right = right_signal(slot);
        for (unsigned i = 0; i < block_frames; ++i) {
            const float level = gain * voice_gains_[i];
            const float l = left[i] * level, r = right[i] * level;
            if (add) { mix_buses_[bus].left[i] += l; mix_buses_[bus].right[i] += r; }
            else { mix_buses_[bus].left[i] = l; mix_buses_[bus].right[i] = r; }
        }
    }
}

bool SynthVoice::render_stereo_node(int slot, Voice& voice, float* left, float* right,
                                  const float* input, const float* input_right, bool shared)
{
    const auto& in = plan_.inputs[slot];
    const auto signal = [this](int at, bool r = false) -> const float* {
        return at < 0 ? silence_ : r ? right_signal(at) : signals_[at].data();
    };
    const auto* env = shared ? nullptr : &env_values_;
    switch (slot) {
    case pan: case pan2: case pan3: {
        const unsigned at = slot - pan;
        const unsigned port = pan_position + 2 * at;
        Pan::process(input, left, right, block_frames,
            pans_[at].gains(modulation(port, env), modulation(port + 1, env)), voice.pans[at]);
        break;
    }
    case stereo_join: {
        std::copy_n(input, block_frames, left);
        std::copy_n(in[1] < 0 ? input : signal(in[1]), block_frames, right);
        break;
    }
    case mono_down:
        for (unsigned i = 0; i < block_frames; ++i) left[i] = (input[i] + input_right[i]) * 0.5f;
        break;
    case stereo_mix:
        for (unsigned i = 0; i < block_frames; ++i) {
            left[i] = stereo_mixer_.sum(input[i], signal(in[1])[i], signal(in[2])[i]);
            right[i] = stereo_mixer_.sum(input_right[i], signal(in[1], true)[i], signal(in[2], true)[i]);
        }
        break;
    case chorus: case rotary: case delay:
        std::copy_n(input, block_frames, left);
        if (right != left) std::copy_n(input_right, block_frames, right);
        if (slot == chorus) chorus_.process(left, right, block_frames, modulation(chorus_rate), modulation(chorus_depth), modulation(chorus_mix));
        else if (slot == rotary) rotary_.process(left, right, block_frames, modulation(rotary_speed), modulation(rotary_depth), modulation(rotary_mix));
        else if (plan_.stereo_graph || delay_.stereo_mode()) delay_.process_stereo(left, right, block_frames, modulation(delay_time), modulation(delay_time_r),
            modulation(delay_feedback), modulation(delay_mix), transport_.bpm());
        else {
            delay_.process(left, block_frames, modulation(delay_time), modulation(delay_feedback), modulation(delay_mix), transport_.bpm());
            std::copy_n(left, block_frames, right);
        }
        break;
    default: return false;
    }
    left[block_frames] = left[block_frames - 1];
    if (right != left) right[block_frames] = right[block_frames - 1];
    return true;
}

void SynthVoice::render_shared()
{
    for (unsigned bus = 0; bus < plan_.mix_count; ++bus) {
        const int slot = plan_.mix_slots[bus];
        std::copy_n(mix_buses_[bus].left.data(), block_frames + 1, signals_[slot].data());
        if (right_signal(slot) != signals_[slot].data())
            std::copy_n(mix_buses_[bus].right.data(), block_frames + 1, right_signal(slot));
    }
    const auto signal = [this](int slot) -> const float* { return slot < 0 ? silence_ : signals_[slot].data(); };
    for (unsigned step = 0; step < plan_.count; ++step) {
        const int slot = plan_.order[step];
        if (!plan_.shared[slot]) continue;
        const auto& in = plan_.inputs[slot];
        float* output = signals_[slot].data();
        const float* input = signal(in[0]);
        const float* r = in[0] < 0 ? silence_ : right_signal(in[0]);
        if (render_stereo_node(slot, post_voice_, output, right_signal(slot), input, r, true)) continue;
        switch (slot) {
        case mixer:
            for (unsigned i = 0; i < block_frames; ++i) output[i] = mixer_.sum(input[i], signal(in[1])[i], signal(in[2])[i]);
            break;
        case filter:
            std::copy_n(input, block_frames + 1, output);
            if (filter_.enabled()) { post_voice_.note = last_note_; filter_voice(post_voice_, output); }
            break;
        case disto:
            for (unsigned i = 0; i < block_frames; ++i) output[i] = distortion_.process(input[i] / 32767) * 32767;
            break;
        case vca: {
            const float target = vca_.gain(modulation(vca_gain), 1);
            const float delta = (target - post_voice_.vca_gain) / block_frames;
            for (unsigned i = 0; i < block_frames; ++i) { post_voice_.vca_gain += delta; output[i] = input[i] * post_voice_.vca_gain; }
            post_voice_.vca_gain = target;
            break;
        }
        case ring_mod: {
            const float mix = ring_.mix(modulation(ring_mix));
            for (unsigned i = 0; i < block_frames; ++i)
                output[i] = input[i] + mix * (input[i] * signal(in[1])[i] / 32767 - input[i]);
            break;
        }
        case wavefolder: {
            const auto settings = folder_.settings(modulation(fold_drive), modulation(fold_symmetry), modulation(fold_mix));
            for (unsigned i = 0; i < block_frames; ++i)
                output[i] = Wavefolder::process(input[i] / 32767, settings, post_voice_.folder) * 32767;
            break;
        }
        default: std::fill_n(output, block_frames, 0.f); break;
        }
    }
    const int out = plan_.output;
    std::copy_n(signal(out), block_frames, sum_);
    std::copy_n(out < 0 ? silence_ : right_signal(out), block_frames, sum_right_);
}

template<bool mixed, bool add>
void SynthVoice::render_active(Voice& voice, float gain)
{
    const auto next_sample = [&]() {
        if (voice.osc_fm) {
            const float second = voice.oscillators[1].next();
            const float first = voice.oscillators[0].next_pm(oscillator_fm(voice,voice.oscillators[1].raw()));
            if constexpr (mixed) return mixer_.sum(first,second,voice.oscillators[2].next());
            return first;
        }
        if constexpr (mixed) {
            return mixer_.sum(voice.oscillators[0].next(), voice.oscillators[1].next(),
                              voice.oscillators[2].next());
        }
        return voice.oscillators[0].next();
    };
    if (filter_.enabled()) {
        for (unsigned frame = 0; frame < block_frames; ++frame) {
            mono_[frame] = next_sample();
        }
        filter_voice(voice, mono_);
        for (unsigned frame = 0; frame < block_frames; ++frame) {
            const float sample = mono_[frame] * voice.envelope.next() * gain * voice_gains_[frame];
            if constexpr (add) {
                sum_[frame] += sample;
            } else {
                sum_[frame] = sample;
            }
        }
        return;
    }
    for (unsigned frame = 0; frame < block_frames; ++frame) {
        const float sample = next_sample() * voice.envelope.next() * gain * voice_gains_[frame];
        if constexpr (add) {
            sum_[frame] += sample;
        } else {
            sum_[frame] = sample;
        }
    }
}
