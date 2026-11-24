#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

class NotePitch {
public:
    void reset() { valid_ = false; remaining_ = 0; }
    void target(float note, bool glide, unsigned milliseconds, unsigned sample_rate)
    {
        target_ = note;
        if (!valid_ || !glide || !milliseconds) current_ = note;
        remaining_ = current_ == note ? 0 : milliseconds * sample_rate / 1000;
        step_ = remaining_ ? (note - current_) / remaining_ : 0;
        valid_ = true;
    }
    float value() const { return current_; }
    float advance(unsigned frames)
    {
        const unsigned count = std::min(frames, remaining_);
        current_ += step_ * count;
        remaining_ -= count;
        if (!remaining_) current_ = target_;
        return current_;
    }
private:
    float current_ = 60, target_ = 60, step_ = 0;
    unsigned remaining_ = 0;
    bool valid_ = false;
};

class HeldNotes {
public:
    struct Note { uint8_t source, channel, pitch, velocity; };
    void on(Note note)
    {
        off(note.source, note.channel, note.pitch);
        if (count_ == notes_.size()) { std::move(notes_.begin() + 1, notes_.end(), notes_.begin()); --count_; }
        notes_[count_++] = note;
    }
    void off(uint8_t source, uint8_t channel, uint8_t pitch)
    {
        erase([=](Note n) { return n.source == source && n.channel == channel && n.pitch == pitch; });
    }
    void clear(uint8_t source, int channel)
    {
        erase([=](Note n) { return n.source == source && (channel < 0 || n.channel == channel); });
    }
    void filter_channel(unsigned channel)
    {
        if (channel) erase([=](Note n) { return n.channel + 1 != channel; });
    }
    bool empty() const { return count_ == 0; }
    Note last() const { return notes_[count_ - 1]; }
    void reset() { count_ = 0; }
private:
    template<class Predicate> void erase(Predicate matches)
    {
        count_ = std::remove_if(notes_.begin(), notes_.begin() + count_, matches) - notes_.begin();
    }
    std::array<Note, 64> notes_{};
    unsigned count_ = 0;
};
