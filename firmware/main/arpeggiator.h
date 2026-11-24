#pragma once

#include "parameter.h"
#include "transport.h"

#include <array>

class Arpeggiator {
public:
    ModuleParameters controls() { return {"Arpeggiator", parameters_, nullptr, nullptr, &hidden_parameters_}; }
    void set_external_trigger(bool connected)
    {
        hidden_parameters_.store(connected ? 1u << 2 : 0, std::memory_order_relaxed);
    }
    bool enabled() const { return parameters_[0].get(); }
    unsigned division() const { return parameters_[2].get(); }
    struct Note {
        uint8_t source, channel, pitch, velocity;
    };
    void reset()
    {
        count_ = 0;
        sounding_ = false;
        step_ = 0;
        last_step_ = 0;
        was_enabled_ = enabled();
    }
    void note_on(Note note)
    {
        const auto matches = [note](const Note& n) { return n.source == note.source && n.channel == note.channel && n.pitch == note.pitch; };
        const auto found = std::find_if(notes_.begin(), notes_.begin() + count_, matches);
        if (found != notes_.begin() + count_) {
            *found = note;
            return;
        }
        if (count_ < notes_.size()) {
            if (!count_) step_ = 0;
            notes_[count_++] = note;
        }
    }
    void note_off(Note note)
    {
        const auto last = notes_.begin() + count_;
        const auto next = std::remove_if(notes_.begin(), last, [note](const Note& n) {
            return n.source == note.source && n.channel == note.channel && n.pitch == note.pitch;
        });
        count_ = next - notes_.begin();
    }
    void clear_source(uint8_t source, int channel)
    {
        const auto last = notes_.begin() + count_;
        count_ = std::remove_if(notes_.begin(), last, [=](const Note& n) {
            return n.source == source && (channel < 0 || n.channel == channel);
        }) - notes_.begin();
    }
    void filter_channel(unsigned channel)
    {
        if (!channel) return;
        const auto last = notes_.begin() + count_;
        count_ = std::remove_if(notes_.begin(), last, [=](const Note& n) {
            return n.channel + 1 != channel;
        }) - notes_.begin();
    }
    template<class On, class Off>
    void process(const Transport& transport, bool trigger, bool reset, bool run, On on, Off off)
    {
        const bool enable = enabled();
        if (sounding_ && (!enable || !run || !count_ || transport.beat() >= release_ || trigger || reset)) {
            off(current_);
            sounding_ = false;
        }
        if (!enable) {
            if (was_enabled_) count_ = 0;
            was_enabled_ = false;
            return;
        }
        was_enabled_ = true;
        if (reset) step_ = 0;
        if (!count_ || !run || !trigger) return;
        const unsigned octaves = parameters_[4].get();
        const unsigned total = count_ * octaves;
        const unsigned mode = parameters_[1].get();
        unsigned at = step_ % total;
        if (mode == 1) at = total - 1 - at;
        else if (mode == 2 && total > 1) {
            at = step_ % (total * 2 - 2);
            if (at >= total) at = total * 2 - 2 - at;
        } else if (mode == 4) {
            random_ ^= random_ << 13;
            random_ ^= random_ >> 17;
            random_ ^= random_ << 5;
            at = random_ % total;
        }
        Note chosen = notes_[at % count_];
        if (mode != 3) {
            std::array<unsigned, 64> order{};
            for (unsigned i = 0; i < count_; ++i) order[i] = i;
            std::sort(order.begin(), order.begin() + count_, [this](unsigned a, unsigned b) {
                return notes_[a].pitch < notes_[b].pitch;
            });
            chosen = notes_[order[at % count_]];
        }
        const unsigned pitch = chosen.pitch + 12 * (at / count_);
        ++step_;
        if (pitch > 127) return;
        chosen.pitch = pitch;
        current_ = chosen;
        on(chosen);
        sounding_ = true;
        const double interval = last_step_ > 0 && transport.beat() > last_step_
            ? transport.beat() - last_step_ : division_beats[division()];
        last_step_ = transport.beat();
        release_ = transport.beat() + interval * parameters_[3].get() * 0.01;
    }
private:
    static constexpr const char* switches[] = {"Off", "On"};
    static constexpr const char* modes[] = {"Up", "Down", "Up-down", "Played", "Random"};
    Parameter parameters_[5] = {
        {"On", 0, 1, 1, 1, "", switches}, {"Mode", 0, 4, 1, 0, "", modes},
        {"Division", 0, 9, 1, 2, "", division_names},
        {"Gate", 1, 99, 1, 65, "%"}, {"Octaves", 1, 4, 1, 1},
    };
    std::array<Note, 64> notes_{};
    std::atomic<unsigned> hidden_parameters_{0};
    unsigned count_ = 0, step_ = 0;
    uint32_t random_ = 0x12345678;
    bool sounding_ = false, was_enabled_ = true;
    Note current_{};
    double release_ = 0, last_step_ = 0;
};
