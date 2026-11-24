#pragma once

#include "adsr.h"
#include "effects.h"
#include "filter.h"
#include "mixer.h"
#include "osc.h"
#include "patch_plan.h"
#include "patch_settings.h"
#include "sampler.h"
#include "granulizer.h"
#include "control_modules.h"
#include "arpeggiator.h"
#include "va_modules.h"
#include "note_play.h"
#include "stereo_effects.h"
#include "fm4.h"
#include "reverb.h"
#include "simple_effects.h"
#include "tb303_filter.h"
#include "live_input.h"

class SynthVoice {
public:
    static constexpr unsigned sample_rate = 44100;
    static constexpr unsigned block_frames = 32;

    explicit SynthVoice(const PatchSettings& settings) : settings_(settings) {}
    void prepare();
    ModuleParameters oscillator_controls(unsigned index);
    void load_wave(unsigned index, const WaveData& wave) { oscillators_[index].load_wave(wave.pcm); }
    float preview_wave(unsigned index, uint32_t phase) const { return oscillators_[index].preview_custom(phase); }
    ModuleParameters envelope_controls() { return envelope_.controls(); }
    ModuleParameters filter_controls() { return filter_.controls(); }
    Filter::Response base_filter_response() const { return filter_.base_response(); }
    bool request_filter_response() { return filter_.request_response(); }
    void cancel_filter_response() { filter_.cancel_response(); }
    bool read_filter_response(Filter::Response& response) { return filter_.read_response(response); }
    void publish_filter_response(); // Called only for a queued display request on the audio core.
    ModuleParameters mixer_controls() { return mixer_.controls(); }
    ModuleParameters lfo_controls(unsigned index = 0)
    {
        constexpr const char* names[] = {"LFO", "LFO 2", "LFO 3"};
        return lfos_[index].controls(names[index]);
    }
    ModuleParameters env_controls(unsigned index)
    {
        constexpr const char* names[] = {"Env 1", "Env 2"};
        return envs_[index].modulation_controls(names[index]);
    }
    ModuleParameters distortion_controls() { return distortion_.controls(); }
    ModuleParameters delay_controls() { return delay_.controls(); }
    ModuleParameters sampler_controls(unsigned index)
    {
        constexpr const char* names[] = {"Sampler", "Sampler 2", "Sampler 3"};
        return samplers_[index].controls(names[index]);
    }
    void set_sample(unsigned index, std::span<const int16_t> samples)
    {
        if (index < samplers_.size()) samplers_[index].set_data(samples);
        else {
            granular_.set_sample(samples);
            granular_preview_.set_data(samples);
        }
        if (index == preview_index_) sample_preview_.reset();
    }
    ModuleParameters granulizer_controls() { return granular_.controls(); }
    ModuleParameters clock_controls(unsigned index) { return clocks_[index].controls(index ? "Clock 2" : "Clock"); }
    ModuleParameters arp_controls() { return arp_.controls(); }
    ModuleParameters utility_controls(unsigned index)
    {
        constexpr const char* names[] = {"S&H", "S&H 2", "Random", "Random 2", "Gain/Offset", "Slew"};
        return utilities_[index].controls(names[index]);
    }
    ModuleParameters noise_controls() { return noise_.controls(); }
    ModuleParameters sub_controls() { return sub_.controls(); }
    ModuleParameters vca_controls() { return vca_.controls(); }
    ModuleParameters ring_controls() { return ring_.controls(); }
    ModuleParameters wavefolder_controls() { return folder_.controls(); }
    ModuleParameters pan_controls(unsigned i) { return pans_[i].controls(i == 0 ? "Pan" : i == 1 ? "Pan 2" : "Pan 3"); }
    ModuleParameters chorus_controls() { return chorus_.controls(); }
    ModuleParameters rotary_controls() { return rotary_.controls(); }
    ModuleParameters stereo_mix_controls() { return stereo_mixer_.controls("Stereo Mix"); }
    ModuleParameters decimator_controls() { return decimator_.controls(); }
    ModuleParameters bitcrusher_controls() { return bitcrusher_.controls(); }
    ModuleParameters compressor_controls() { return compressor_.controls(); }
    ModuleParameters tb303_controls() { return tb303_.controls(); }
    ModuleParameters input_controls() { return input_.controls(); }
    bool uses_audio_input() const { return plan_.has(audio_input); }
    ModuleParameters reverb_controls() { return reverb_.controls(); }
    unsigned effect_allocated_bytes(int slot) const
    {
        return slot == delay ? delay_.allocated_bytes() : slot == chorus ? chorus_.allocated_bytes() :
            slot == rotary ? rotary_.allocated_bytes() : slot == reverb ? reverb_.allocated_bytes() : 0;
    }
    ModuleParameters fm4_controls() { return fm4_.controls(); }
    void receive_clock(const ClockEvent& event);
    void set_time(uint64_t now) { now_ = now; }
    void midi_note_on(uint8_t source, uint8_t channel, uint8_t note, uint8_t velocity);
    void midi_note_off(uint8_t source, uint8_t channel, uint8_t note);
    void midi_clear(uint8_t source, int channel = -1, bool immediate = false);
    void play_sample(unsigned index);
    void stop_sample_preview() { sample_preview_.reset(); }
    void set_library_sample(std::span<const int16_t> samples)
    {
        if (preview_index_ == sample_slots.size()) sample_preview_.reset();
        library_preview_.set_data(samples);
    }
    bool prepare_patch(const AudioPlan& plan)
    {
        return (!plan.has(delay) || delay_.prepare()) && (!plan.has(chorus) || chorus_.prepare()) &&
               (!plan.has(rotary) || rotary_.prepare()) && (!plan.has(reverb) || reverb_.prepare());
    }
    void select_plan(const AudioPlan& plan);
    void note_on(uint8_t source, uint8_t channel, uint8_t note, uint8_t velocity);
    void note_off(uint8_t source, uint8_t channel, uint8_t note);
    void all_notes_off(uint8_t source, int channel = -1, bool immediate = false);
    void render(int16_t* stereo, const int16_t* input = nullptr);

private:
    struct Voice {
        Osc::Voice oscillators[3];
        Osc::Voice sub;
        Fm4::Voice fm;
        float osc_fm_amount = 0, osc_fm_step = 0, osc_fm_target = 0;
        bool osc_fm = false;
        NotePitch pitch;
        uint32_t noise = 1;
        float noise_level = 0, vca_gain = 0;
        float glide_start = 1, glide_end = 1;
        Wavefolder::State folder;
        Decimator::State decimator;
        Bitcrusher::State crusher;
        Tb303Filter::State acid;
        std::array<Pan::Gains, 3> pans{};
        std::array<Sampler::Voice, sampler_slots.size()> samplers;
        Granulizer::Voice grains;
        Adsr::Voice envelope{sample_rate};
        Adsr::Voice mod_envelopes[2]{
            Adsr::Voice{sample_rate / block_frames}, Adsr::Voice{sample_rate / block_frames}};
        Filter::Voice filter;
        uint32_t age = 0;
        float gain = 0.0f;
        float target_gain = 0.0f;
        float velocity = 0.0f;
        uint8_t source = 0;
        uint8_t channel = 0;
        uint8_t note = 0;
        bool held = false;
        std::array<ControlModule::State, utility_slots.size()> controls{};
    };

    bool active(const Voice& voice) const;
    void release(Voice& voice);
    void reset(Voice& voice);
    void update_limits();
    float note_ratio(float note) const;
    const Modulation* modulation(unsigned port, const std::array<Modulation, 2>* env = nullptr) const;
    void update_lfos();
    void update_timing();
    void configure_transport();
    void update_arp_mode();
    void update_controls(Voice* voice = nullptr);
    const Modulation* source_modulation(int slot, const std::array<Modulation, 2>* env) const;
    bool event_input(int slot, unsigned port, bool fallback = false) const;
    static int utility_kind(int slot) { return slot <= sample_hold2 ? 0 : slot <= random2 ? 1 : slot == gain_offset ? 2 : 3; }
    void update_oscillators(std::array<Osc::Settings, 3>& osc, const std::array<Modulation, 2>* env) const;
    void gate_envelopes(Voice& voice, bool on, bool repeated = false);
    void filter_voice(Voice& voice, float* samples);
    void render_shared(const int16_t* input);
    bool render_stereo_node(int slot, Voice& voice, float* left, float* right,
                            const float* in_left, const float* in_right, bool shared);
    void collect_voice(float gain, bool add);
    void render_insert(int slot, Voice& voice, const float* input, float* output, bool shared);
    std::array<float, block_frames> voice_gains_{};
    float* right_signal(int slot);
    static uint32_t oscillator_fm(Voice& voice, float signal)
    {
        voice.osc_fm_amount += voice.osc_fm_step;
        return Osc::phase_modulation(signal, unsigned(std::clamp(voice.osc_fm_amount, 0.f, 512.f)));
    }
    Granulizer::Settings grain_settings(const std::array<Modulation, 2>* envelopes = nullptr) const;
    template<bool mixed, bool add>
    void render_active(Voice& voice, float gain);
    template<bool add>
    void render_graph(Voice& voice, float gain);
    template<bool add>
    void render_voice(Voice& voice, float gain);
    template<bool add>
    void accumulate(const float* signal, float gain);

    const PatchSettings& settings_;
    Osc oscillators_[3];
    std::array<Sampler, sampler_slots.size()> samplers_;
    std::array<Sampler::Settings, sampler_slots.size()> sampler_block_{};
    Sampler::Voice sample_preview_;
    Granulizer granular_;
    Granulizer::Settings grain_block_{};
    bool grain_envelope_modulation_ = false;
    Sampler granular_preview_;
    Sampler library_preview_;
    unsigned preview_index_ = 0;
    Mixer mixer_;
    Adsr envelope_;
    Filter filter_{sample_rate};
    Lfo lfos_[3];
    Adsr envs_[2];
    Distortion distortion_;
    Delay delay_;
    Transport transport_;
    Arpeggiator arp_;
    Noise noise_;
    Sub sub_;
    Vca vca_;
    RingMod ring_;
    Wavefolder folder_;
    Fm4 fm4_;
    Fm4::Settings fm_block_{};
    std::array<Pan, 3> pans_;
    Chorus chorus_;
    Rotary rotary_;
    Reverb reverb_;
    Decimator decimator_;
    Bitcrusher bitcrusher_;
    Compressor compressor_;
    Tb303Filter tb303_;
    LiveInput input_;
    Mixer stereo_mixer_;
    Voice post_voice_;
    HeldNotes held_notes_;
    bool monophonic_ = false;
    ControlModule clocks_[2]{ControlModule::clock(), ControlModule::clock()};
    ControlModule utilities_[6]{ControlModule::hold(), ControlModule::hold(), ControlModule::random(),
                                ControlModule::random(), ControlModule::gain(), ControlModule::slew()};
    std::array<ControlModule::State, utility_slots.size()> control_states_{};
    std::array<Modulation, utility_slots.size()> control_values_{}, voice_control_values_{};
    std::array<std::array<bool, 3>, 2> clock_outputs_{};
    bool midi_trigger_ = false, arp_trigger_ = false;
    bool arp_enabled_ = false;
    uint64_t now_ = 0;
    std::array<double, 3> lfo_sync_offsets_{};
    std::array<Modulation, 3> lfo_values_{};
    std::array<Modulation, 2> env_values_{};

    std::array<Voice, max_voices> voices_{};
    std::array<Osc::Settings, 3> osc_block_{};
    Adsr::Settings envelope_block_{};
    // Work buffers are shared by voices. ESP-DSP reads one float past the block.
    float mono_[block_frames + 1]{};
    float scratch_[block_frames + 1]{};
    float fm_modulator_[block_frames]{};
    float sum_[block_frames]{};
    float sum_right_[block_frames]{};
    std::array<std::array<float, block_frames + 1>, max_patch_modules> signals_{};
    std::array<std::array<float, block_frames + 1>, stereo_buffer_count> right_signals_{};
    struct MixBus { std::array<float, block_frames + 1> left{}, right{}; };
    std::array<MixBus, max_mix_buses> mix_buses_{};
    static const float silence_[block_frames + 1];
    AudioPlan plan_ = default_audio_plan();
    std::array<uint32_t, 3> lfo_phase_{};
    std::array<float, 128> note_ratios_{};
    uint8_t last_note_ = 60;
    float last_velocity_ = 1;
    uint32_t clock_ = 0;
    unsigned voice_limit_ = 1;
    int midi_channel_ = 0;
    float output_gain_ = 1.0f;
};
