#include "synth_voice.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

static void set(ModuleParameters controls, const char* name, int value)
{
    for (auto& p : controls.parameters) if (std::string_view(p.name) == name) {
        p.set(value); if (controls.on_change) controls.on_change(controls.context); return;
    }
    assert(false);
}
static ModuleParameters controls(SynthVoice& s, int slot)
{
    switch (slot) {
    case oscillator: return s.oscillator_controls(0);
    case oscillator2: return s.oscillator_controls(1);
    case oscillator3: return s.oscillator_controls(2);
    case envelope: return s.envelope_controls();
    case filter: return s.filter_controls();
    case mixer: return s.mixer_controls();
    case lfo: case lfo2: case lfo3: return s.lfo_controls(slot == lfo ? 0 : slot == lfo2 ? 1 : 2);
    case env1: case env2: return s.env_controls(slot - env1);
    case disto: return s.distortion_controls();
    case delay: return s.delay_controls();
    case granulizer: return s.granulizer_controls();
    case clock1: case clock2: return s.clock_controls(slot - clock1);
    case random1: case random2: case slew: return s.utility_controls(slot - sample_hold);
    case noise: return s.noise_controls();
    case ring_mod: return s.ring_controls();
    case wavefolder: return s.wavefolder_controls();
    case pan: case pan2: case pan3: return s.pan_controls(slot - pan);
    case chorus: return s.chorus_controls();
    case rotary: return s.rotary_controls();
    case fm4: return s.fm4_controls();
    default: assert(false); return {};
    }
}
struct Resources {
    std::vector<int16_t> pcm;
    static std::vector<int16_t> read(const char* id) {
        std::ifstream file(std::string("../editor/dist/presets/assets/") + id + ".pcm", std::ios::binary | std::ios::ate);
        assert(file); const auto size = file.tellg();
        std::vector<int16_t> data(size_t(size) / 2);
        file.seekg(0); file.read(reinterpret_cast<char*>(data.data()), size); assert(file);
        return data;
    }
    void wave(SynthVoice& s, unsigned index, const char* id) {
        const auto data = read(id); assert(data.size() == wave_samples);
        WaveData wave{}; std::copy(data.begin(), data.end(), wave.pcm); s.load_wave(index, wave);
    }
    void sample(SynthVoice& s, const char* id) { pcm = read(id); s.set_sample(sample_slots.size(), pcm); }
};
static void modulate(AudioPlan& plan, int from, int to, const char* name)
{
    for (unsigned i = 0; i < std::size(modulation_inputs); ++i)
        if (modulation_inputs[i].slot == to && std::string_view(modulation_inputs[i].name) == name) {
            plan.modulation |= uint64_t{1} << i; plan.sources[i] = from; return;
        }
    assert(false);
}
template<class Configure> static void run_preset(const char* name, int note, Configure configure)
{
    PatchSettings settings;
    auto s = std::make_unique<SynthVoice>(settings);
    s->prepare();
    AudioPlan plan; plan.stereo_graph = true;
    plan.sources.fill(no_module); for (auto& input : plan.inputs) input.fill(no_module);
    Resources resources;
    configure(*s, settings, plan, resources);
    assert(configure_audio_plan(plan)); assert(s->prepare_patch(plan)); s->select_plan(plan);
    s->note_on(1, 0, note, 110);
    std::array<int16_t, 64> output{};
    unsigned peak = 0, clipped = 0;
    double energy = 0;
    // Three seconds held, then two seconds of releases/effect tails.
    for (unsigned block = 0; block < 6891; ++block) {
        if (block == 1700) {
            s->note_on(1, 0, note + 4, 95);
            s->note_on(1, 0, note + 7, 100);
        }
        if (block == 4134) {
            s->note_off(1, 0, note); s->note_off(1, 0, note + 4); s->note_off(1, 0, note + 7);
        }
        s->set_time(uint64_t(block) * 32 * 1000000 / 44100);
        s->render(output.data());
        for (int value : output) { peak = std::max(peak, unsigned(std::abs(value))); clipped += std::abs(value) >= 32760; energy += double(value) * value; }
    }
    std::printf("%-24s peak=%5u rms=%6.1f clipped=%u\n", name, peak, std::sqrt(energy / (6891 * 64)), clipped);
    assert(peak > 40); assert(clipped == 0);
}
int main()
{
#include "../build/preset-bank.inc"
    std::puts("PASS: 40 real-engine preset renders, single notes/chords/releases, audible signal, no full-scale samples");
}
