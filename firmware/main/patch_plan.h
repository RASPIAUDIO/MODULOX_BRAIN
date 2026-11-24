#pragma once

#include <array>
#include <cstdint>

using ModuleMask = uint64_t;
inline constexpr ModuleMask module_bit(unsigned slot) { return ModuleMask{1} << slot; }
inline constexpr unsigned max_patch_modules = 41;
inline constexpr unsigned fm4_parameter_count = 30;
inline constexpr unsigned max_patch_inputs = 3;
inline constexpr unsigned max_mix_buses = 12;
inline constexpr unsigned stereo_buffer_count = 8;

enum PatchModule : int8_t { oscillator, envelope, filter, oscillator2, oscillator3, mixer, lfo,
                           lfo2, lfo3, env1, env2, midi, disto, delay, sampler, sampler2, sampler3, granulizer,
                           clock1, clock2, arp, sample_hold, sample_hold2, random1, random2, gain_offset, slew,
                           noise, sub, vca, ring_mod, wavefolder,
                           pan, pan2, pan3, chorus, rotary, stereo_join, mono_down, stereo_mix, fm4 };
inline constexpr int8_t no_module = -1;
inline constexpr const char* patch_module_types[] = {
    "Osc", "ADSR", "Filter", "Osc", "Osc", "Mix", "LFO", "LFO", "LFO",
    "Env", "Env", "MIDI", "Disto", "Delay", "Sampler", "Sampler", "Sampler", "Granulizer",
    "Clock", "Clock", "Arpeggiator", "S&H", "S&H", "Random", "Random", "Gain/Offset", "Slew",
    "Noise", "Sub", "VCA", "Ring Mod", "Wavefolder",
    "Pan", "Pan", "Pan", "Chorus", "Rotary", "Stereo", "Mono", "Stereo Mix", "FM4"};
inline constexpr unsigned patch_module_inputs[] = {0, 1, 1, 0, 0, 3, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1,
                                                  1, 1, 1, 1, 1, 2, 1, 3, 0};
inline constexpr std::array stereo_slots{delay, pan, pan2, pan3, chorus, rotary, stereo_join, stereo_mix};
inline constexpr int stereo_buffer(int slot)
{
    for (unsigned i = 0; i < stereo_slots.size(); ++i) if (stereo_slots[i] == slot) return i;
    return -1;
}
inline constexpr bool stereo_output(int slot) { return stereo_buffer(slot) >= 0; }
inline constexpr bool stereo_input(int slot) { return slot == delay || slot == chorus || slot == rotary || slot == mono_down || slot == stereo_mix; }
inline constexpr bool shared_effect(int slot) { return slot == delay || slot == chorus || slot == rotary; }
inline constexpr std::array oscillator_slots{oscillator, oscillator2, oscillator3};
inline constexpr std::array sampler_slots{sampler, sampler2, sampler3};
inline constexpr std::array sample_slots{sampler, sampler2, sampler3, granulizer};
inline constexpr bool sample_slot(unsigned slot) { return slot >= sampler && slot <= granulizer; }
inline constexpr std::array lfo_slots{lfo, lfo2, lfo3};
inline constexpr std::array env_slots{env1, env2};
inline constexpr std::array utility_slots{sample_hold, sample_hold2, random1, random2, gain_offset, slew};
inline constexpr bool envelope_source(int slot) { return slot == env1 || slot == env2; }
inline constexpr bool utility_source(int slot) { return slot >= sample_hold && slot <= slew; }
inline constexpr bool clock_module(int slot) { return slot == clock1 || slot == clock2; }
inline constexpr bool midi_source(int slot) { return slot == midi || slot == arp; }
inline constexpr bool modulation_source(int8_t slot)
{
    return (slot >= lfo && slot <= env2) || utility_source(slot);
}
inline constexpr bool audio_source(int8_t slot)
{
    return slot != no_module && !modulation_source(slot) && !midi_source(slot) && !clock_module(slot);
}
inline constexpr unsigned module_limit(PatchModule slot)
{
    return slot == oscillator || slot == lfo || slot == sampler || slot == pan ? 3
        : slot == env1 || slot == clock1 || slot == sample_hold || slot == random1 ? 2 : 1;
}
inline constexpr int8_t output_slot = max_patch_modules;
struct ModulationInput {
    int8_t slot;
    const char* name;
};
enum ModulationPort {
    global_pitch, output_level, osc_pitch, osc_level, osc_pwm,
    cutoff_port = osc_pitch + 3 * oscillator_slots.size(), resonance_port,
    disto_mode, disto_drive, disto_mix, delay_time, delay_feedback, delay_mix,
    grain_start, grain_end, grain_size, grain_density, grain_speed, grain_spray, grain_window, grain_pitch, grain_level,
    noise_level, sub_pitch, sub_level, vca_gain, ring_mix, fold_drive, fold_symmetry, fold_mix,
    pan_position, pan_level, pan2_position, pan2_level, pan3_position, pan3_level,
    chorus_rate, chorus_depth, chorus_mix, delay_time_r, rotary_speed, rotary_depth, rotary_mix,
    osc_fm, fm4_pitch, fm4_depth, fm4_feedback, fm4_level
};
inline constexpr ModulationInput modulation_inputs[] = {
    {output_slot, "Pitch"}, {output_slot, "Level"},
    {oscillator, "Pitch"}, {oscillator, "Level"}, {oscillator, "PWM"},
    {oscillator2, "Pitch"}, {oscillator2, "Level"}, {oscillator2, "PWM"},
    {oscillator3, "Pitch"}, {oscillator3, "Level"}, {oscillator3, "PWM"},
    {filter, "Cutoff"}, {filter, "Reso"},
    {disto, "Mode"}, {disto, "Drive"}, {disto, "Mix"},
    {delay, "Time"}, {delay, "Feedback"}, {delay, "Mix"},
    {granulizer, "Start"}, {granulizer, "End"}, {granulizer, "Size"},
    {granulizer, "Density"}, {granulizer, "Speed"}, {granulizer, "Spray"},
    {granulizer, "Window"}, {granulizer, "Pitch"}, {granulizer, "Level"},
    {noise, "Level"}, {sub, "Pitch"}, {sub, "Level"}, {vca, "Gain"}, {ring_mod, "Mix"},
    {wavefolder, "Drive"}, {wavefolder, "Symmetry"}, {wavefolder, "Mix"},
    {pan, "Pan"}, {pan, "Level"}, {pan2, "Pan"}, {pan2, "Level"}, {pan3, "Pan"}, {pan3, "Level"},
    {chorus, "Rate"}, {chorus, "Depth"}, {chorus, "Mix"}, {delay, "Time R"},
    {rotary, "Speed"}, {rotary, "Depth"}, {rotary, "Mix"},
    {oscillator, "FM"}, {fm4, "Pitch"}, {fm4, "Depth"}, {fm4, "Feedback"}, {fm4, "Level"},
};
static_assert(std::size(modulation_inputs) == fm4_level + 1 && std::size(modulation_inputs) <= 64);
static_assert(max_patch_modules <= 64 && stereo_slots.size() == stereo_buffer_count);
enum MidiPort { midi_pitch, midi_velocity, midi_gate, midi_trigger };
inline constexpr const char* midi_ports[] = {"Pitch", "Velocity", "Gate", "Trigger"};
enum ControlInput { cv_value, control_trigger, control_reset, control_run };
inline constexpr const char* control_ports[] = {"Value", "Trigger", "Reset", "Run"};
inline constexpr const char* event_ports[] = {"Trigger", "Reset", "Run"};
inline constexpr bool control_input(int slot, unsigned port)
{
    if (port == cv_value) return slot == sample_hold || slot == sample_hold2 || slot == gain_offset || slot == slew;
    if (port == control_trigger) return slot == sample_hold || slot == sample_hold2 || slot == random1 || slot == random2 || slot == arp;
    if (port == control_reset) return utility_source(slot) || (slot >= lfo && slot <= lfo3) || slot == arp;
    return port == control_run && ((slot >= lfo && slot <= lfo3) || slot == arp);
}
inline constexpr bool midi_input(int8_t slot, unsigned port)
{
    if (slot == vca || slot == noise) return port == midi_velocity;
    if (slot == oscillator || slot == oscillator2 || slot == oscillator3 ||
        slot == sub || slot == fm4 || (slot >= lfo && slot <= lfo3)) {
        return port == 0 || port == 1 || port == 3;
    }
    return (slot == env1 || slot == env2) && (port == midi_gate || port == midi_trigger);
}

struct AudioPlan {
    enum class Path { graph, basic, triple };
    using Inputs = std::array<int8_t, max_patch_inputs>;

    std::array<Inputs, max_patch_modules> inputs{};
    std::array<int8_t, max_patch_modules> order{};
    unsigned count = 0;
    ModuleMask modules = 0;
    bool stereo_graph = false;
    bool mix_graph = false;
    std::array<bool, max_patch_modules> shared{};
    std::array<int8_t, max_patch_modules> mix_bus = [] { std::array<int8_t, max_patch_modules> a{}; a.fill(-1); return a; }();
    std::array<int8_t, max_mix_buses> mix_slots{};
    unsigned mix_count = 0;
    bool output_stereo() const { return stereo_graph && stereo_output(output); }
    unsigned oscillators = 0;
    unsigned samplers = 0;
    bool has_envelope = false;
    bool has_filter = false;
    uint64_t modulation = 0;
    std::array<int8_t, std::size(modulation_inputs)> sources{};
    std::array<uint8_t, max_patch_modules> midi_inputs{};
    struct ControlLink { int8_t source = no_module; uint8_t output = 0; };
    std::array<std::array<ControlLink, 4>, max_patch_modules> controls{};
    std::array<int8_t, utility_slots.size()> control_order{};
    unsigned control_count = 0;
    std::array<bool, utility_slots.size()> voice_controls{};
    bool per_voice_source(int slot) const
    {
        return envelope_source(slot) || (utility_source(slot) && voice_controls[slot - sample_hold]);
    }
    int8_t output = no_module;
    Path path = Path::graph;
    bool has(int slot) const { return modules & module_bit(slot); }
    bool explicit_midi() const { return has(midi) || has(arp); }
    bool midi_to(int slot, unsigned port) const { return midi_inputs[slot] & (1u << port); }
};

inline bool configure_audio_plan(AudioPlan& plan)
{
    plan.shared.fill(false);
    plan.mix_bus.fill(-1);
    plan.mix_count = 0;
    plan.mix_graph = plan.stereo_graph;
    for (unsigned i = 0; i < plan.count; ++i) {
        const int slot = plan.order[i];
        plan.shared[slot] = shared_effect(slot);
        for (int input : plan.inputs[slot]) if (input >= 0) plan.shared[slot] = plan.shared[slot] || plan.shared[input];
        if (plan.shared[slot]) {
            plan.mix_graph = true;
            if (slot == envelope) return false; // ADSR is a per-note audio gate.
        }
    }
    const auto frontier = [&](int slot) {
        if (slot < 0 || plan.shared[slot] || plan.mix_bus[slot] >= 0) return true;
        if (plan.mix_count == max_mix_buses) return false;
        plan.mix_slots[plan.mix_count] = slot;
        plan.mix_bus[slot] = plan.mix_count++;
        return true;
    };
    if (plan.mix_graph) {
        for (unsigned i = 0; i < plan.count; ++i) {
            const int slot = plan.order[i];
            if (plan.shared[slot]) for (int input : plan.inputs[slot]) if (!frontier(input)) return false;
        }
        if (!frontier(plan.output)) return false;
    }
    return true;
}

inline AudioPlan default_audio_plan()
{
    AudioPlan plan;
    plan.modules = (1u << oscillator) | (1u << filter) | (1u << envelope);
    plan.oscillators = 1;
    plan.has_envelope = plan.has_filter = true;
    plan.path = AudioPlan::Path::basic;
    return plan;
}
