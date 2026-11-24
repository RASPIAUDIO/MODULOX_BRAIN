#include "synth_voice.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

static void set(ModuleParameters controls, const char* name, int value)
{
    for (auto& p : controls.parameters) if (std::string_view(p.name) == name) { p.set(value); return; }
    assert(false);
}

static std::vector<float> impulse(Reverb& effect, unsigned frames = 44100 * 4)
{
    effect.reset();
    std::vector<float> audio(frames * 2);
    for (unsigned base = 0; base < frames; base += 32) {
        float l[32]{}, r[32]{};
        if (base == 0) l[0] = 16000;
        const unsigned count = std::min(32u, frames - base);
        effect.process(l, r, count);
        for (unsigned i = 0; i < count; ++i) {
            assert(std::isfinite(l[i]) && std::isfinite(r[i]));
            assert(std::abs(l[i]) < 32768 && std::abs(r[i]) < 32768);
            audio[2 * (base + i)] = l[i]; audio[2 * (base + i) + 1] = r[i];
        }
    }
    return audio;
}

static double energy(const std::vector<float>& audio, unsigned first, unsigned last)
{
    double sum = 0;
    for (unsigned i = 2 * first; i < 2 * last; ++i) sum += double(audio[i]) * audio[i];
    return sum;
}

int main()
{
    static_assert(Reverb::buffer_bytes < 100 * 1024);
    Reverb effect;
    assert(effect.prepare()); assert(effect.prepare());
    set(effect.controls(), "Mix", 0);
    auto dry = impulse(effect, 8192);
    assert(dry[0] == 16000 && energy(dry, 1, 8192) == 0);
    set(effect.controls(), "Mix", 100);
    set(effect.controls(), "Damping", 0);
    set(effect.controls(), "Pre-delay", 0);
    const auto wet = impulse(effect);
    assert(energy(wet, 0, 1493) == 0);
    assert(energy(wet, 1493, 44100) > 1000);
    assert(energy(wet, 3 * 44100, 4 * 44100) < energy(wet, 0, 44100) * 0.001);
    double difference = 0;
    for (unsigned i = 0; i < wet.size(); i += 2) difference += std::abs(wet[i] - wet[i + 1]);
    assert(difference > 1000); // Mono impulse generates a stereo tail.
    set(effect.controls(), "Decay", 200);
    const auto short_tail = impulse(effect);
    assert(energy(short_tail, 22050, 44100) < energy(wet, 22050, 44100) * 0.01);
    set(effect.controls(), "Decay", 2000);
    set(effect.controls(), "Damping", 100);
    const auto damped = impulse(effect);
    assert(energy(damped, 0, 44100) < energy(wet, 0, 44100) * 0.5);
    set(effect.controls(), "Damping", 0);
    set(effect.controls(), "Width", 0);
    const auto mono = impulse(effect, 8192);
    for (unsigned i = 0; i < mono.size(); i += 2) assert(mono[i] == mono[i + 1]);
    set(effect.controls(), "Width", 100);
    set(effect.controls(), "Pre-delay", 100);
    const auto delayed = impulse(effect, 12000);
    assert(energy(delayed, 0, 1493 + 4410) == 0);
    assert(energy(delayed, 1493 + 4410, 12000) > 1000);
    // Reset masks stale PSRAM without clearing buffers in the audio task.
    effect.reset();
    for (unsigned block = 0; block < 500; ++block) {
        float l[32]{}, r[32]{}; effect.process(l, r, 32);
        for (unsigned i = 0; i < 32; ++i) assert(l[i] == 0 && r[i] == 0);
    }
    // CV clamps all controls at their bounds, including fractional predelay ramps.
    Modulation high{1, 1, 1}, low{0, 1, -1};
    for (unsigned block = 0; block < 1000; ++block) {
        float l[32]{}, r[32]{}; l[0] = 1000;
        const auto* cv = block % 2 ? &high : &low;
        effect.process(l, r, 32, cv, cv, cv, cv, cv);
        for (unsigned i = 0; i < 32; ++i) assert(std::isfinite(l[i]) && std::isfinite(r[i]));
    }
    // Longest decay, including rapid parameter changes and sustained full scale.
    set(effect.controls(), "Decay", 10000);
    for (unsigned block = 0; block < 30000; ++block) {
        if (block % 1000 == 0) {
            set(effect.controls(), "Damping", (block / 1000) % 2 ? 100 : 0);
            set(effect.controls(), "Pre-delay", (block / 1000) % 2 ? 0 : 100);
        }
        float l[32], r[32];
        std::fill_n(l, 32, block < 15000 ? 32767.f : 0.f);
        std::fill_n(r, 32, block < 15000 ? -32767.f : 0.f);
        effect.process(l, r, 32);
        for (unsigned i = 0; i < 32; ++i)
            assert(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i]) < 1e7f && std::abs(r[i]) < 1e7f);
    }
    // Actual graph, eight voices, Reverb -> Chorus -> Mono: tails must survive Note Off.
    PatchSettings settings; settings.parameters[polyphony].set(8);
    auto synth = std::make_unique<SynthVoice>(settings); synth->prepare();
    AudioPlan plan; plan.stereo_graph = true; plan.oscillators = 1; plan.has_envelope = true;
    for (auto& in : plan.inputs) in.fill(no_module);
    plan.sources.fill(no_module);
    plan.order[0] = oscillator; plan.order[1] = envelope; plan.order[2] = stereo_join;
    plan.order[3] = reverb; plan.order[4] = chorus; plan.order[5] = mono_down;
    plan.count = 6;
    for (unsigned i = 0; i < plan.count; ++i) {
        plan.modules |= module_bit(plan.order[i]);
        if (i) plan.inputs[plan.order[i]][0] = plan.order[i - 1];
    }
    plan.output = mono_down;
    assert(configure_audio_plan(plan)); assert(plan.shared[reverb] && plan.shared[mono_down]);
    assert(synth->prepare_patch(plan));
    set(synth->envelope_controls(), "Release", 0);
    set(synth->reverb_controls(), "Mix", 100);
    synth->select_plan(plan);
    for (unsigned i = 0; i < 8; ++i) synth->note_on(1, 0, 48 + i, 100);
    int16_t output[64];
    for (unsigned i = 0; i < 1000; ++i) synth->render(output);
    for (unsigned i = 0; i < 8; ++i) synth->note_off(1, 0, 48 + i);
    double tail = 0;
    for (unsigned i = 0; i < 1000; ++i) {
        synth->render(output);
        for (int sample : output) { tail += double(sample) * sample; assert(std::abs(sample) < 32767); }
    }
    assert(tail > 10000);
    std::printf("PASS: reverb impulse, decay, stereo/mono, predelay, dry, reset, stability, 8-voice graph/tails; buffers=%u bytes\n", Reverb::buffer_bytes);
}
