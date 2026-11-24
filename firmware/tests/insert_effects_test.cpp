#include "synth_voice.h"
#include "rosic_TeeBeeFilter.h"
#include <cassert>
#include <cstdio>
#include <memory>

static rosic::TeeBeeFilter reference; // Static storage initializes upstream constructor state.
static void set(ModuleParameters controls, const char* name, int value)
{
    for (auto& p : controls.parameters) if (std::string_view(p.name) == name) { p.set(value); return; }
    assert(false);
}
static double filter_energy(Tb303Filter& filter, float frequency)
{
    Tb303Filter::State state;
    double energy = 0;
    for (unsigned block = 0; block < 700; ++block) {
        float in[32], out[32];
        for (unsigned i = 0; i < 32; ++i) in[i] = .1f * std::sin(6.283185307f * frequency * (block * 32 + i) / 44100);
        Tb303Filter::process(in, out, 32, filter.settings(nullptr, nullptr), state);
        for (float x : out) { assert(std::isfinite(x)); if (block > 500) energy += x * x; }
    }
    return energy;
}

int main()
{
    static_assert(tb303_resonance == 63 && std::size(modulation_inputs) == 64);
    float in[32], out[32]; for (unsigned i = 0; i < 32; ++i) in[i] = (int(i) - 16) / 32.f;
    Decimator decimate; Decimator::State d;
    Decimator::process(in, out, 32, {.25f, 1}, d);
    for (unsigned i = 0; i < 32; ++i) assert(out[i] == in[i - i % 4]);
    for (unsigned i = 0; i < 32; ++i) in[i] += 1;
    Decimator::process(in, out, 32, {.25f, 1}, d);
    for (unsigned i = 0; i < 32; ++i) assert(out[i] == in[i - i % 4]); // Cross-block cadence.
    d = {}; Decimator::process(in, out, 32, {1, 1}, d);
    for (unsigned i = 0; i < 32; ++i) assert(in[i] == out[i]);
    d = {}; Decimator::process(in, out, 32, {.01f, 0}, d);
    for (unsigned i = 0; i < 32; ++i) assert(in[i] == out[i]);
    Bitcrusher crush; Bitcrusher::State b;
    Bitcrusher::process(in, out, 32, {2, 1, false}, b);
    for (float x : out) assert(std::abs(x) <= 1 && std::abs(x * 2 - std::round(x * 2)) < 1e-6);
    b = {}; Bitcrusher::process(in, out, 32, {32768, 1, true}, b);
    for (unsigned i = 0; i < 32; ++i) assert(in[i] == out[i]);
    b = {}; Bitcrusher::process(in, out, 32, {2, 0, false}, b);
    for (unsigned i = 0; i < 32; ++i) assert(in[i] == out[i]);
    for (unsigned i = 0; i < 32; ++i) in[i] = -1 + 2.f * i / 31;
    b = {}; Bitcrusher::process(in, out, 32, {2, 1, false}, b);
    bool codes[4]{};
    for (float x : out) { const int code = int(std::round(x * 2)); assert(code >= -2 && code <= 1); codes[code + 2] = true; }
    for (bool used : codes) assert(used); // Exactly four signed levels at two bits.

    Compressor comp;
    set(comp.controls(), "Threshold", -20); set(comp.controls(), "Attack", 1);
    float left[32], right[32];
    for (unsigned block = 0; block < 500; ++block) {
        std::fill_n(left, 32, .8f * 32767); std::fill_n(right, 32, .2f * 32767);
        comp.process(left, right, 32);
        for (unsigned i = 0; i < 32; ++i) assert(std::abs(left[i] - 4 * right[i]) < .01f);
    }
    const float expected = .8f * 32767 * std::pow(.1f / .8f, .75f);
    assert(std::abs(left[31] - expected) < .5f);
    // Release recovers the gain on a quiet signal; ratio=1 and Mix=0 are dry.
    const float compressed = left[31] / (.8f * 32767);
    for (unsigned block = 0; block < 1000; ++block) {
        std::fill_n(left, 32, 100.f); std::fill_n(right, 32, 25.f); comp.process(left, right, 32);
    }
    assert(left[31] > compressed * 100 && left[31] > 99.9f);
    comp.reset(); set(comp.controls(), "Ratio", 1);
    std::fill_n(left, 32, 20000.f); std::fill_n(right, 32, 5000.f); comp.process(left, right, 32);
    assert(left[0] == 20000 && right[31] == 5000);
    set(comp.controls(), "Mix", 0); set(comp.controls(), "Makeup", 24);
    comp.process(left, right, 32); assert(left[31] == 20000 && right[0] == 5000);

    // Compare float TB core against the actual unchanged MIT upstream source.
    double worst = 0;
    for (float cutoff : {200.f, 1000.f, 8000.f, 20000.f}) for (float resonance : {0.f, 35.f, 95.f}) {
        reference.setSampleRate(TeeBee303::rate); reference.setCutoff(cutoff, false);
        reference.setResonance(resonance, true); reference.reset();
        TeeBee303::State state;
        const auto c = TeeBee303::coefficients(cutoff, resonance);
        double difference = 0, energy = 0;
        for (unsigned i = 0; i < 8192; ++i) {
            const float x = .01f * std::sin(i * .07f) + (i == 0 ? .01f : 0);
            const double a = reference.getSample(x), b = TeeBee303::next(x, c, state);
            assert(std::isfinite(b)); difference += (a - b) * (a - b); energy += a * a;
        }
        const double relative = std::sqrt(difference / std::max(energy, 1e-20));
        worst = std::max(worst, relative);
        assert(relative < .001); // Float feedback sensitivity at high resonance.
    }
    Tb303Filter acid;
    set(acid.controls(), "Reso", 0);
    const double low = filter_energy(acid, 220), high = filter_energy(acid, 8000);
    assert(low > high * 100);
    for (int cutoff : {200, 1000, 20000}) for (int resonance : {0, 95}) for (int drive : {0, 24}) {
        set(acid.controls(), "Cutoff", cutoff); set(acid.controls(), "Reso", resonance); set(acid.controls(), "Drive", drive);
        Tb303Filter::State state;
        for (unsigned block = 0; block < 500; ++block) {
            std::fill_n(in, 32, block % 3 ? 1.f : -1.f);
            Tb303Filter::process(in, out, 32, acid.settings(nullptr, nullptr), state);
            for (float x : out) assert(std::isfinite(x) && std::abs(x) <= 8.001f);
        }
    }
    set(acid.controls(), "Mix", 0); Tb303Filter::State dry;
    Tb303Filter::process(in, out, 32, acid.settings(nullptr, nullptr), dry);
    for (unsigned i = 0; i < 32; ++i) assert(in[i] == out[i]);

    PatchSettings settings; settings.parameters[polyphony].set(8);
    auto synth = std::make_unique<SynthVoice>(settings); synth->prepare();
    AudioPlan plan; plan.stereo_graph = true; plan.oscillators = 1; plan.has_envelope = true;
    for (auto& input : plan.inputs) input.fill(no_module); plan.sources.fill(no_module);
    constexpr std::array order{oscillator, tb303, decimator, bitcrusher, envelope, stereo_join, compressor};
    plan.count = order.size(); plan.output = compressor;
    for (unsigned i = 0; i < order.size(); ++i) {
        plan.order[i] = order[i]; plan.modules |= module_bit(order[i]);
        if (i) plan.inputs[order[i]][0] = order[i - 1];
    }
    plan.modules |= module_bit(env1);
    plan.sources[tb303_resonance] = env1; plan.modulation = uint64_t{1} << tb303_resonance;
    assert(configure_audio_plan(plan) && plan.shared[compressor] && !plan.shared[tb303]);
    assert(synth->prepare_patch(plan)); synth->select_plan(plan);
    for (unsigned i = 0; i < 8; ++i) synth->note_on(1, 0, 48 + i, 100);
    int16_t pcm[64]; double audible = 0;
    for (unsigned block = 0; block < 1600; ++block) {
        if (block == 800) for (unsigned i = 0; i < 8; ++i) synth->note_off(1, 0, 48 + i);
        synth->render(pcm); for (int x : pcm) { assert(std::abs(x) < 32767); audible += double(x) * x; }
    }
    assert(audible > 10000);
    // The same mono inserts must also run after a shared stereo effect, using
    // PCM-scaled buses. Their dry paths must preserve every output sample.
    AudioPlan post; post.stereo_graph = true; post.oscillators = 1; post.has_envelope = true;
    for (auto& input : post.inputs) input.fill(no_module); post.sources.fill(no_module);
    constexpr std::array post_order{oscillator, envelope, stereo_join, compressor, mono_down, decimator, bitcrusher, tb303, pan};
    post.count = post_order.size(); post.output = pan;
    for (unsigned i = 0; i < post_order.size(); ++i) {
        post.order[i] = post_order[i]; post.modules |= module_bit(post_order[i]);
        if (i) post.inputs[post_order[i]][0] = post_order[i - 1];
    }
    assert(configure_audio_plan(post) && post.shared[tb303]);
    set(synth->decimator_controls(), "Mix", 0); set(synth->bitcrusher_controls(), "Mix", 0);
    set(synth->tb303_controls(), "Mix", 0); set(synth->compressor_controls(), "Ratio", 1);
    assert(synth->prepare_patch(post)); synth->select_plan(post);
    auto dry_synth = std::make_unique<SynthVoice>(settings); dry_synth->prepare();
    AudioPlan dry_plan = post;
    dry_plan.count = 6; constexpr std::array dry_order{oscillator, envelope, stereo_join, compressor, mono_down, pan};
    dry_plan.modules = 0;
    for (unsigned i = 0; i < dry_order.size(); ++i) { dry_plan.order[i] = dry_order[i]; dry_plan.modules |= module_bit(dry_order[i]); }
    dry_plan.inputs[pan][0] = mono_down;
    assert(configure_audio_plan(dry_plan) && dry_synth->prepare_patch(dry_plan));
    set(dry_synth->compressor_controls(), "Ratio", 1); dry_synth->select_plan(dry_plan);
    for (unsigned i = 0; i < 8; ++i) { synth->note_on(1, 0, 48 + i, 100); dry_synth->note_on(1, 0, 48 + i, 100); }
    int16_t reference_pcm[64];
    for (unsigned block = 0; block < 500; ++block) {
        synth->render(pcm); dry_synth->render(reference_pcm);
        for (unsigned i = 0; i < 64; ++i) assert(pcm[i] == reference_pcm[i]);
    }
    post.modules |= module_bit(lfo); post.sources[tb303_resonance] = lfo; post.modulation = uint64_t{1} << tb303_resonance;
    set(synth->decimator_controls(), "Mix", 100); set(synth->bitcrusher_controls(), "Mix", 100); set(synth->tb303_controls(), "Mix", 100);
    synth->select_plan(post); synth->note_on(1, 0, 48, 100);
    double post_energy = 0;
    for (unsigned block = 0; block < 500; ++block) { synth->render(pcm); for (int x : pcm) post_energy += double(x) * x; }
    assert(post_energy > 10000);
    std::printf("PASS: decimation/quantization/dry paths, stereo compression/ratio/release, TB upstream reference (worst RMS error %.3g), 4x stability/response, 8-voice/shared graphs and CV bit 63, exact PCM dry path; TB state=%zu bytes\n", worst, sizeof(Tb303Filter::State));
}
