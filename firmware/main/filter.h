#pragma once

#include "lfo.h"
#include "display_mailbox.h"

#include <array>

class Filter {
    using Coefficients = std::array<float, 5>;
    struct Settings {
        int mode = 0;
        int stages = 0;
        std::array<Coefficients, 2> coefficients{};
    };

public:
    struct Response {
        int mode = 0;
        int stages = 0;
        std::array<Coefficients, 2> coefficients{};
        bool live = false;
    };
    explicit Filter(unsigned sample_rate) : sample_rate_(sample_rate) {}

    using History = std::array<std::array<float, 2>, 2>;
    struct Voice {
        History history{};
        unsigned revision = 0;
        unsigned modulation_revision = 0;
        float note = -1;
        unsigned smoothing_blocks = 0;
        Settings active;
        Settings target;
    };
    ModuleParameters controls();
    Response base_response() const;
    bool request_response() { return display_response_.request(); }
    void cancel_response() { display_response_.cancel(); }
    bool response_wanted() const { return display_response_.wanted(); }
    bool read_response(Response& response) { return display_response_.read(response); }
    void publish_response(const Voice* voice);
    void reset_plan();
    void restart_modulation() { modulation_block_ = 0; }
    void begin_block(const Modulation* cutoff_mod, const Modulation* resonance_mod,
                     bool voice_cutoff = false, bool voice_resonance = false);
    bool enabled() const { return active_.stages || transition_; }
    // Each buffer has one readable padding float after the last frame for ESP-DSP.
    void process(Voice& voice, float note, float* samples, float* scratch, int frames,
                 const Modulation* cutoff_mod = nullptr, const Modulation* resonance_mod = nullptr);

private:
    enum Index { mode, order, cutoff, resonance };
    enum Mode { bypass, low_pass, high_pass, band_pass };
    static constexpr const char* mode_names[] = {"Bypass", "Low-pass", "High-pass", "Band-pass"};
    static constexpr const char* order_names[] = {"2 poles", "4 poles"};
    Parameter parameters_[4] = {
        {"Mode", bypass, band_pass, 1, bypass, "", mode_names},
        {"Order", 0, 1, 1, 0, "", order_names},
        {"Cutoff", 20, 18000, 5, 2000, "Hz", {}, 16},
        {"Reso", 0, 100, 1, 0, "%"},
    };

    static constexpr float minimum_hz = 20.0f;
    static constexpr float maximum_hz = 18000.0f;
    static constexpr float cutoff_span_octaves = 4.0f;
    static constexpr int tracking_reference = 60;

    Settings topology() const;
    float resonance_value(const Modulation* modulation) const;
    void generate(Settings& settings, float hz, float resonance) const;
    void update_tracked(Voice& voice, float note, const Modulation* cutoff_mod,
                        const Modulation* resonance_mod) const;
    static void interpolate(Settings& active, const Settings& target, float mix);
    void publish_parameters();
    static void apply(float* samples, int frames, Settings& settings, History& history);

    const unsigned sample_rate_;
    // Single producer (controls) and single consumer (audio). Each owns one
    // buffer and exchanges it with the spare. The flag marks unread settings.
    static constexpr unsigned pending = 4;
    std::array<Settings, 3> settings_{};
    std::atomic<unsigned> spare_{1};
    unsigned write_slot_ = 2;
    unsigned read_slot_ = 0;
    Settings active_{};
    Settings previous_{};
    Settings modulation_target_{};
    float cutoff_start_octaves_ = 0;
    float cutoff_end_octaves_ = 0;
    float cutoff_position_ = 0;
    float resonance_target_ = 0;
    unsigned revision_ = 0;
    unsigned modulation_revision_ = 0;
    unsigned modulation_block_ = 0;
    bool tracking_ = false;
    bool note_tracking_ = false;
    bool transition_ = false;
    int smoothing_blocks_ = 0;
    float mix_ = 0;
    DisplayMailbox<Response> display_response_;
};
