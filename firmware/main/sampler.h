#pragma once

#include "parameter.h"

#include <cmath>
#include <cstdint>

class Sampler {
public:
    enum Index { root, pitch, fine, level, mode, start, end, channel };
    enum Mode { one_shot, gate, loop, drum };

    ModuleParameters controls(const char* name)
    {
        static constexpr unsigned menu_order[] = {start, end, root, pitch, fine, level, mode, channel};
        static constexpr ParameterPage pages[] = {{"",0,2},{"",2,4},{"",6,2}};
        return {name, parameters_, nullptr, nullptr, nullptr, pages, menu_order};
    }
    void set_data(std::span<const int16_t> samples) { samples_ = samples; }
    bool accepts(uint8_t note, uint8_t midi_channel) const
    {
        const int selected_channel = parameters_[channel].get();
        return samples_.size() >= 2 && (!selected_channel || midi_channel + 1 == selected_channel) &&
               (parameters_[mode].get() != drum || note == parameters_[root].get());
    }

    struct Settings {
        int cents;
        int root_note;
        float gain;
    };

    Settings settings() const
    {
        return {parameters_[pitch].get() * 100 + parameters_[fine].get(), parameters_[root].get(),
                parameters_[level].get() * (1.0f / 100)};
    }

    class Voice {
    public:
        bool active() const { return running_; }
        void reset() { running_ = false; }
        void release()
        {
            if (mode_ == gate) reset();
            else if (mode_ == loop) mode_ = one_shot;
        }
        void note_on(const Sampler& source, bool preview = false)
        {
            mode_ = preview ? one_shot : source.parameters_[mode].get();
            const unsigned count = source.samples_.size();
            first_ = std::min<unsigned>(source.parameters_[start].get() * count / 100, count - 2);
            last_ = std::clamp<unsigned>(source.parameters_[end].get() * count / 100, first_ + 2, count);
            phase_ = uint64_t{first_} << 16;
            running_ = true;
        }
        void render(float* target, unsigned frames, std::span<const int16_t> pcm, Settings settings,
                    int note, std::span<const float, 128> note_ratios, float pitch_cents)
        {
            if (!running_) {
                std::fill_n(target, frames, 0.0f);
                return;
            }
            const int cents = settings.cents + static_cast<int>(pitch_cents);
            if (cents != last_cents_) {
                last_cents_ = cents;
                tuning_ = std::exp2(cents / 1200.0f);
            }
            const float ratio = mode_ == drum || note == settings.root_note ? 1.0f
                : note_ratios[note] / note_ratios[settings.root_note];
            const auto step = static_cast<uint32_t>(65536 * tuning_ * ratio + 0.5f);
            const float gain = settings.gain * (1.0f / 32768);
            const uint64_t start_phase = uint64_t{first_} << 16;
            const uint64_t end_phase = uint64_t{last_} << 16;
            const uint64_t span = end_phase - start_phase;
            unsigned frame = 0;
            while (frame < frames && running_) {
                const unsigned index = phase_ >> 16;
                const unsigned next = index + 1 < last_ ? index + 1 : mode_ == loop ? first_ : index;
                const float fraction = (phase_ & 0xffffu) * (1.0f / 65536);
                target[frame++] = (pcm[index] + (pcm[next] - pcm[index]) * fraction) * gain;
                phase_ += step;
                if (phase_ >= end_phase) {
                    if (mode_ == loop) {
                        phase_ -= span;
                        if (phase_ >= end_phase) phase_ = start_phase + (phase_ - start_phase) % span;
                    }
                    else running_ = false;
                }
            }
            std::fill(target + frame, target + frames, 0.0f);
        }
    private:
        uint64_t phase_ = 0;
        unsigned first_ = 0;
        unsigned last_ = 0;
        int mode_ = one_shot;
        int last_cents_ = 0;
        float tuning_ = 1;
        bool running_ = false;
    };

    std::span<const int16_t> samples() const { return samples_; }
private:
    static constexpr const char* modes[] = {"One-shot", "Gate", "Loop", "Drum"};
    Parameter parameters_[8] = {
        {"Root", 0, 127, 1, 60}, {"Pitch", -24, 24, 1, 0, "st"},
        {"Fine", -100, 100, 1, 0, "ct"}, {"Level", 0, 100, 1, 100, "%"},
        {"Mode", 0, 3, 1, one_shot, "", modes}, {"Start", 0, 99, 1, 0, "%"},
        {"End", 1, 100, 1, 100, "%"}, {"MIDI Ch", 0, 16, 1, 0},
    };
    std::span<const int16_t> samples_;
};
