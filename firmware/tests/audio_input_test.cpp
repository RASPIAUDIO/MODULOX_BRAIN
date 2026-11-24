#include "synth_voice.h"
#include "audio_input_stream.h"
#include "recording.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <string_view>

static void set(ModuleParameters controls, const char* name, int value)
{
    for (auto& p : controls.parameters) if (std::string_view(p.name) == name) { p.set(value); return; }
    assert(false);
}

static AudioPlan chain(std::initializer_list<PatchModule> nodes)
{
    AudioPlan plan;
    plan.stereo_graph = true;
    plan.sources.fill(no_module);
    for (auto& input : plan.inputs) input.fill(no_module);
    int previous = no_module;
    for (const auto slot : nodes) {
        plan.order[plan.count++] = slot;
        plan.modules |= module_bit(slot);
        if (previous >= 0) plan.inputs[slot][0] = previous;
        plan.has_filter |= slot == filter;
        plan.has_envelope |= slot == envelope;
        plan.oscillators += slot == oscillator;
        previous = slot;
    }
    plan.output = previous;
    return plan;
}

int main()
{
    static_assert(audio_input == 46 && tb303 == 45 && reverb == 41);
    static_assert(std::size(modulation_inputs) == 64);
    // Actual DMA ownership logic, including pause/record/restart and driver errors.
    AudioInputStream<32> stream;
    using Mode = AudioInputStream<32>::Mode;
    unsigned enabled = 0, disabled = 0;
    bool enable_ok = true, disable_ok = true;
    const auto enable = [&] { ++enabled; return enable_ok; };
    const auto disable = [&] { ++disabled; return disable_ok; };
    assert(stream.select(Mode::live, enable, disable));
    assert(stream.select(Mode::live, enable, disable) && enabled == 1 && disabled == 0);
    unsigned reads = 0;
    const auto read = [&](int16_t* pcm, size_t size) {
        assert(size == 128); ++reads;
        std::fill_n(pcm, 64, int16_t(reads));
        return AudioInputStream<32>::Read{size, false};
    };
    const auto* latest = stream.latest(read);
    assert(reads == 4 && latest && latest[0] == 4); // Newest, bounded backlog drain.
    const auto empty = [](int16_t*, size_t) { return AudioInputStream<32>::Read{0, false}; };
    assert(!stream.latest(empty) && !stream.failed()); // Never repeats the previous live block.
    assert(!stream.latest([](int16_t*, size_t) { return AudioInputStream<32>::Read{4, false}; }) && stream.failed());
    assert(!stream.latest([](int16_t*, size_t) { return AudioInputStream<32>::Read{0, true}; }) && stream.failed());
    assert(stream.latest(read) && !stream.failed()); // Recovery on the next complete block.
    assert(stream.select(Mode::recording, enable, disable) && enabled == 2 && disabled == 1);
    reads = 0; assert(!stream.latest(read) && reads == 0);
    assert(stream.select(Mode::off, enable, disable) && disabled == 2);
    enable_ok = false; assert(!stream.select(Mode::live, enable, disable));
    assert(stream.mode() == Mode::off);
    enable_ok = true; assert(stream.select(Mode::live, enable, disable));
    disable_ok = false; assert(!stream.select(Mode::recording, enable, disable));
    assert(stream.mode() == Mode::live); // Does not double-enable a running RX.
    disable_ok = true; assert(stream.select(Mode::off, enable, disable));

    PatchSettings settings;
    set(settings.controls(), "Voices", 8);
    auto synth = std::make_unique<SynthVoice>(settings);
    synth->prepare();
    auto plan = chain({audio_input, filter, stereo_join});
    assert(configure_audio_plan(plan) && plan.shared[audio_input] && plan.shared[filter]);
    assert(plan.mix_count == 0 && synth->prepare_patch(plan));
    set(synth->filter_controls(), "Mode", 0);
    synth->select_plan(plan);
    assert(synth->uses_audio_input());
    int16_t mic[64], out[64];
    for (unsigned i = 0; i < 32; ++i) { mic[2 * i] = int(i) * 61 - 1000; mic[2 * i + 1] = 24000; }
    // Same-block, bit-exact mono path. No note is needed and R is not the mic.
    for (unsigned block = 0; block < 20; ++block) {
        if (block == 5) for (unsigned i = 0; i < 8; ++i) synth->note_on(1, 0, 48 + i, 100);
        synth->render(out, mic);
        for (unsigned i = 0; i < 32; ++i) assert(out[2 * i] == mic[2 * i] && out[2 * i + 1] == mic[2 * i]);
    }
    synth->render(out); for (auto x : out) assert(x == 0);
    set(synth->input_controls(), "Level", 0);
    synth->render(out, mic); assert(out[62] == 0 && out[63] == 0);
    synth->render(out, mic); for (auto x : out) assert(x == 0);
    set(synth->input_controls(), "Level", 50);
    synth->render(out, mic); synth->render(out, mic);
    for (unsigned i = 0; i < 32; ++i) assert(out[2 * i] == int16_t(mic[2 * i] * .5f));

    // Microphone and a polyphonic synth branch meet only once at the frontier.
    plan = chain({oscillator, envelope, mixer, stereo_join});
    plan.order = {}; plan.order[0] = audio_input; plan.order[1] = oscillator;
    plan.order[2] = envelope; plan.order[3] = mixer; plan.order[4] = stereo_join;
    plan.count = 5; plan.modules |= module_bit(audio_input);
    plan.inputs[mixer][1] = audio_input;
    assert(configure_audio_plan(plan) && plan.mix_count == 1 && plan.shared[mixer]);
    set(synth->input_controls(), "Level", 100);
    set(synth->oscillator_controls(0), "Level", 1);
    auto reference = std::make_unique<SynthVoice>(settings); reference->prepare();
    set(reference->oscillator_controls(0), "Level", 1);
    synth->select_plan(plan); reference->select_plan(plan);
    for (unsigned i = 0; i < 8; ++i) { synth->note_on(1, 0, 48 + i, 100); reference->note_on(1, 0, 48 + i, 100); }
    set(synth->mixer_controls(), "In 1", 100); set(reference->mixer_controls(), "In 1", 100);
    set(synth->mixer_controls(), "In 2", 100); set(reference->mixer_controls(), "In 2", 100);
    // Opposite-sign sums can differ by up to two LSB after int16 truncation.
    int16_t dry[64];
    for (unsigned block = 0; block < 100; ++block) {
        synth->render(out, mic); reference->render(dry);
        for (unsigned i = 0; i < 32; ++i) {
            if (std::abs((out[2 * i] - dry[2 * i]) - mic[2 * i] / 3.f) > 2) {
                std::fprintf(stderr, "mixed block %u frame %u out=%d reference=%d mic=%d expected=%g\n", block, i, out[2*i], dry[2*i], mic[2*i], mic[2*i]/3.f);
                assert(false);
            }
        }
    }
    // Per-note gates cannot gate a source which has no associated MIDI voice.
    auto invalid = chain({audio_input, envelope, stereo_join});
    assert(!configure_audio_plan(invalid));
    // Existing recorder still captures only L; stopping/restarting RX is tested above.
    Recording recording; int16_t take[32]{};
    recording.arm(take); recording.set(RecordingState::recording);
    assert(recording.capture(mic));
    for (unsigned i = 0; i < 32; ++i) assert(take[i] == mic[2 * i]);
    // Live effects run without notes and keep their tails after capture stops.
    plan = chain({audio_input, tb303, stereo_join, reverb});
    assert(configure_audio_plan(plan) && plan.shared[tb303] && synth->prepare_patch(plan));
    set(synth->tb303_controls(), "Mix", 100);
    set(synth->reverb_controls(), "Mix", 100);
    synth->select_plan(plan);
    double tail = 0;
    for (unsigned block = 0; block < 800; ++block) {
        synth->render(out, block < 100 ? mic : nullptr);
        for (auto x : out) { assert(std::abs(int(x)) < 32767); if (block > 100) tail += double(x) * x; }
    }
    assert(tail > 1000);
    synth->select_plan(default_audio_plan());
    assert(!synth->uses_audio_input()); synth->render(out, mic);
    for (auto x : out) assert(x == 0);
    std::puts("PASS: live INPUT without MIDI, exact same-block PCM/L-only, level ramp, 8-voice mixed graph, TB/Reverb tails, bounded newest-block RX, underrun/error recovery, recording ownership and patch stop");
}
