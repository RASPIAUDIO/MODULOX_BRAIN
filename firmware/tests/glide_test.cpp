#include "synth_voice.h"

#include <cassert>
#include <cstdio>
#include <memory>

static void set(ModuleParameters controls, const char* name, int value)
{
    for (auto& parameter : controls.parameters) {
        if (std::string_view(parameter.name) == name) {
            parameter.set(value);
            if (controls.on_change) controls.on_change(controls.context);
            return;
        }
    }
    assert(false);
}

int main()
{
    // A live filter must not snap to the new key tracking coefficients.
    Filter filter{44100};
    set(filter.controls(), "Mode", 1);
    set(filter.controls(), "Cutoff", 420);
    filter.reset_plan();
    Filter::Voice voice;
    Modulation cutoff_env{1, 0, 0.2f};
    std::array<float, 33> audio{}, scratch{};
    const auto block = [&](float note) {
        audio.fill(0.1f);
        filter.begin_block(nullptr, nullptr, true, false);
        filter.process(voice, note, audio.data(), scratch.data(), 32, &cutoff_env);
        for (auto sample : audio) assert(std::isfinite(sample));
    };
    block(36);
    const float before = voice.active.coefficients[0][0];
    block(48);
    const float first = voice.active.coefficients[0][0];
    const float target = voice.target.coefficients[0][0];
    assert(first > before && first < target);
    for (int i = 0; i < 8; ++i) block(48);
    assert(std::abs(voice.active.coefficients[0][0] - target) < 1e-8f);
    block(48.5f);
    assert(voice.target.coefficients[0][0] > target); // Fractional glide pitch follows too.

    // Render the real engine: a legato note with lower velocity starts at
    // the same instantaneous sample, then settles smoothly to its new level.
    for (bool graph : {false, true}) {
        PatchSettings settings;
        settings.parameters[play_mode].set(1);
        settings.parameters[glide_mode].set(2);
        settings.parameters[glide_time].set(65);
        auto reference = std::make_unique<SynthVoice>(settings);
        auto changed = std::make_unique<SynthVoice>(settings);
        auto plan = default_audio_plan();
        if (graph) {
            plan.path = AudioPlan::Path::graph;
            plan.stereo_graph = true;
            plan.modules |= module_bit(pan);
            plan.order[0] = oscillator; plan.order[1] = envelope; plan.order[2] = pan;
            plan.count = 3;
            for (auto& inputs : plan.inputs) inputs.fill(no_module);
            plan.inputs[envelope][0] = oscillator;
            plan.inputs[pan][0] = envelope;
            plan.output = pan;
        }
        for (auto* synth : {reference.get(), changed.get()}) {
            synth->prepare();
            set(synth->envelope_controls(), "Attack", 0);
            synth->select_plan(plan);
            synth->note_on(1, 0, 36, 127);
        }
        std::array<int16_t, 64> a{}, b{};
        for (int i = 0; i < 500; ++i) { reference->render(a.data()); changed->render(b.data()); }
        changed->note_on(1, 0, 48, 40);
        reference->render(a.data()); changed->render(b.data());
        assert(std::abs(int(a[0]) - int(b[0])) < 60);
        int peak = 0;
        for (int i = 0; i < 500; ++i) {
            changed->render(b.data());
            if (i > 450) for (auto sample : b) peak = std::max(peak, std::abs(int(sample)));
        }
        assert(peak > 500 && peak < 2700);
    }
    std::puts("PASS: filter note transition, fractional key tracking, mono glide/velocity (basic and stereo graph)");
}
