#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

inline constexpr const char* division_names[] = {
    "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 bar", "2 bars"
};
inline constexpr double division_beats[] = {0.125, 1.0/6, 0.25, 1.0/3, 0.5, 2.0/3, 1, 2, 4, 8};
inline constexpr const char* sync_ppqn_names[] = {"1", "2", "4", "24"};
inline constexpr unsigned sync_ppqn_values[] = {1, 2, 4, 24};

struct ClockEvent {
    uint64_t timestamp;
    uint8_t source;  // 1 USB, 2 DIN, 3 jack
    uint8_t status;  // MIDI realtime, or F8 for a jack edge
    uint16_t position = 0;
};

class Transport {
public:
    struct Config {
        unsigned bpm, source, run, ppqn, loss;
        bool operator==(const Config&) const = default;
    };

    void configure(Config config)
    {
        if (config.source != config_.source || config.ppqn != config_.ppqn) {
            clear_external();
            beat_ = 0;
            ++generation_;
        }
        config_ = config;
        if (!config.source) tempo_ = config.bpm;
    }

    void receive(const ClockEvent& event)
    {
        if (!config_.source || event.source != config_.source) return;
        if (event.status == 0xfa) {
            beat_ = pulse_beat_ = 0;
            ++generation_;
            stopped_ = false;
            pending_start_ = true;
            external_running_ = false;
        } else if (event.status == 0xfb) {
            stopped_ = false;
            pending_start_ = true;
        } else if (event.status == 0xfc) {
            external_running_ = false;
            stopped_ = true;
        } else if (event.status == 0xf2) {
            beat_ = pulse_beat_ = event.position * 0.25;
            ++generation_;
        } else if (event.status == 0xf8) {
            const double ppqn = event.source == 3 ? sync_ppqn_values[config_.ppqn] : 24;
            const uint64_t interval = last_pulse_ && event.timestamp > last_pulse_ ? event.timestamp - last_pulse_ : 0;
            // Ignore duplicate edges/bounce without moving the accepted timestamp.
            if (interval && interval < 1000) return;
            if (interval && interval < 8'000'000) {
                const double measured = 60'000'000.0 / (interval * ppqn);
                if (measured >= 10 && measured <= 600) {
                    tempo_ = locked_ ? tempo_ + 0.2 * (measured - tempo_) : measured;
                    locked_ = true;
                }
            }
            if (last_pulse_ && !pending_start_ && !stopped_) pulse_beat_ += 1 / ppqn;
            if (!stopped_) {
                if (!external_running_ && !pending_start_) {
                    beat_ = pulse_beat_ = 0;
                    ++generation_;
                }
                external_running_ = true;
                pending_start_ = false;
                // Gentle correction avoids a backward phase jump on incoming jitter.
                correction_ = std::clamp(pulse_beat_ - beat_, -0.25, 0.25);
            }
            last_pulse_ = event.timestamp;
        }
    }

    void advance(unsigned frames, unsigned sample_rate, uint64_t now)
    {
        reset_ = observed_generation_ != generation_;
        observed_generation_ = generation_;
        if (config_.source && last_pulse_) {
            const unsigned ppqn = config_.source == 3 ? sync_ppqn_values[config_.ppqn] : 24;
            const uint64_t timeout = std::max<uint64_t>(1'000'000, 180'000'000.0 / (tempo_ * ppqn));
            if (now > last_pulse_ && now - last_pulse_ > timeout) {
                locked_ = false;
                if (!config_.loss) external_running_ = false;
                last_pulse_ = 0;
                pending_start_ = false;
            }
        }
        previous_ = beat_;
        running_ = config_.run && (!config_.source || external_running_);
        started_ = running_ && !was_running_;
        was_running_ = running_;
        if (running_) {
            double amount = tempo_ / 60 * frames / sample_rate;
            if (config_.source) {
                const double correction = std::clamp(correction_ * 0.04, -amount * 0.25, amount * 0.25);
                amount += correction;
                correction_ -= correction;
            }
            beat_ += amount;
        }
    }

    bool pulse(unsigned division) const
    {
        const double length = division_beats[division];
        return running_ && (reset_ || started_ || std::floor(beat_ / length) != std::floor(previous_ / length));
    }
    bool running() const { return running_; }
    bool reset() const { return reset_; }
    double beat() const { return beat_; }
    double bpm() const { return tempo_; }
    double increment() const { return beat_ - previous_; }
    void disconnect(unsigned source)
    {
        if (source == config_.source) {
            clear_external();
            running_ = false;
        }
    }
private:
    void clear_external()
    {
        last_pulse_ = 0;
        correction_ = 0;
        external_running_ = locked_ = stopped_ = pending_start_ = false;
        pulse_beat_ = 0;
    }
    Config config_{120, 0, 1, 1, 0};
    double beat_ = 0, previous_ = 0, pulse_beat_ = 0, tempo_ = 120, correction_ = 0;
    uint64_t last_pulse_ = 0;
    unsigned generation_ = 1, observed_generation_ = 0;
    bool external_running_ = false, stopped_ = false, pending_start_ = false, locked_ = false;
    bool running_ = false, was_running_ = false, reset_ = false, started_ = false;
};
