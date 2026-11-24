#pragma once

#include <algorithm>
#include <cstdint>

// WM8960 rev 4.4, tables 25/26/31 and registers R26/R40/R41/R49/R51.
// Transport is injected so every hot-switch/failure sequence can run on PC.
enum class SpeakerOutput : unsigned { off, left, right, stereo };
struct SpeakerConfig { SpeakerOutput output = SpeakerOutput::off; int db = -30; };
struct SpeakerStatus {
    SpeakerOutput output = SpeakerOutput::off;
    unsigned gain = 0;
    bool fault = false;
    bool available = false;
};

class Wm8960Speaker {
public:
    // Existing DAC, headphone, OUT3 and PLL enables. Only SPKL/SPKR may change.
    static constexpr uint16_t power2_base = 0x1e3;
    static constexpr uint16_t class_d_off = 0x037; // Preserve reserved reset bits.
    static constexpr uint16_t class_d_unity = 0x080; // Reserved bits + 1x AC/DC.
    static constexpr unsigned minimum_gain = 0x30, maximum_gain = 0x79;

    SpeakerStatus status() const { return {output_, gain_, fault_, true}; }

    // Called once per 10 ms on the codec-control task, never the audio core.
    // Each register attempt must have a bounded timeout. A fault latches until
    // a NEW explicit Off request; changing volume/channel cannot auto-restart it.
    template<class Write>
    void service(SpeakerConfig request, uint32_t revision, Write write)
    {
        if (unsigned(request.output) > unsigned(SpeakerOutput::stereo)) request.output = SpeakerOutput::off;
        request.db = std::clamp(request.db, -60, 0);
        const bool changed = revision != revision_;
        revision_ = revision;
        if (fault_) {
            if (request.output == SpeakerOutput::off && changed && shutdown(write)) {
                fault_ = false;
            }
            return;
        }
        if (request.output == SpeakerOutput::off) {
            if (output_ != SpeakerOutput::off && !shutdown(write)) fail(write);
            return;
        }
        if (output_ != request.output) {
            if (!shutdown(write) ||
                !write(0x17, 0x1c0) || // Thermal shutdown remains enabled; no mono-DAC rerouting.
                !write(0x30, 0x002) || // Temperature sensor enabled; jack switching stays off.
                !write(0x33, class_d_unity) || // No supply-dependent speaker boost.
                !write(0x1a, power2_base | power_bits(request.output)) ||
                !write(0x31, class_d_off | (unsigned(request.output) << 6)) ||
                !volume(request.output, minimum_gain, write)) {
                fail(write);
                return;
            }
            output_ = request.output;
            gain_ = minimum_gain;
            return; // Start at -73 dB; ramp begins on the next tick.
        }
        const unsigned target = unsigned(121 + request.db); // -60..0 dB, never +6 dB.
        if (gain_ == target) return;
        const unsigned next = gain_ < target ? gain_ + 1 : gain_ - 1;
        if (!volume(output_, next, write)) fail(write);
        else gain_ = next;
    }

private:
    static uint16_t power_bits(SpeakerOutput output)
    { return (unsigned(output) & 1 ? 0x10 : 0) | (unsigned(output) & 2 ? 0x08 : 0); }

    template<class Write>
    static bool volume(SpeakerOutput output, unsigned gain, Write write)
    {
        const uint16_t left = unsigned(output) & 1 ? gain : 0;
        const uint16_t right = unsigned(output) & 2 ? gain : 0;
        // Stage L, then commit L/R together; zero-cross waiting is disabled.
        const bool l = write(0x28, left);
        const bool r = write(0x29, 0x100 | right);
        return l && r;
    }

    template<class Write>
    bool shutdown(Write write)
    {
        // Do not short-circuit: even a failed volume write must try to turn
        // off Class D and its PGAs. Do not touch headphone volume or clocks.
        bool ok = volume(SpeakerOutput::off, 0, write);
        const bool drivers = write(0x31, class_d_off);
        const bool pgas = write(0x1a, power2_base);
        ok = ok && drivers && pgas;
        if (ok) { output_ = SpeakerOutput::off; gain_ = 0; }
        return ok;
    }

    template<class Write>
    void fail(Write write)
    {
        shutdown(write); // Bounded best effort; a dead bus cannot prove physical Off.
        fault_ = true;
    }

    SpeakerOutput output_ = SpeakerOutput::off;
    unsigned gain_ = 0;
    bool fault_ = false;
    uint32_t revision_ = 0;
};
