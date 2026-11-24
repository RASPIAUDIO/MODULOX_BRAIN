#pragma once

#include "lfo.h"

#include <array>
#include <cmath>
#include <cstdint>

class Granulizer {
public:
    static constexpr unsigned parameter_count = 9;
    static constexpr unsigned max_grains = 8;
    using Modulations = std::array<const Modulation*, parameter_count>;

    ModuleParameters controls() { return {"Granulizer", parameters_}; }
    void set_sample(std::span<const int16_t> pcm) { pcm_ = pcm; }
    bool ready() const { return pcm_.size() >= 2; }
    void prepare()
    {
        for (unsigned i = 0; i < hann_.size(); ++i) {
            hann_[i] = 0.5f - 0.5f * std::cos(6.283185307f * i / (hann_.size() - 1));
        }
    }

    struct Settings {
        unsigned first, last, duration;
        float interval, speed, spray, ratio, gain;
        int window;
    };

    Settings settings(const Modulations& modulation) const
    {
        const auto value = [&](unsigned index) {
            const auto& p = parameters_[index];
            return modulation[index] ? modulation[index]->apply(p.get(), p.minimum, p.maximum,
                                                                  p.maximum - p.minimum) : float(p.get());
        };
        constexpr float rate = 44100;
        const unsigned first = std::min<unsigned>(value(0) * 0.01f * pcm_.size(), pcm_.size() - 2);
        const unsigned last = std::clamp<unsigned>(value(1) * 0.01f * pcm_.size(), first + 2, pcm_.size());
        const unsigned duration = value(2) * (rate / 1000);
        const float density = value(3);
        const float overlap = std::clamp(duration * density / rate, 1.0f, float(max_grains));
        return {first, last, duration, rate / density, value(4) * 0.01f,
                value(5) * 0.01f * (last - first), std::exp2(value(7) / 12),
                value(8) * (0.01f / 32768) / std::sqrt(overlap), int(value(6) + 0.5f)};
    }

    class Voice {
    public:
        void reset()
        {
            for (auto& grain : grains_) grain.remaining = 0;
            active_ = 0;
        }
        bool active() const { return active_ != 0; }
        void trigger(uint32_t seed)
        {
            reset();
            random_ = seed ? seed : 1;
            scan_ = wait_ = 0;
        }

        void render(const Granulizer& source, const Settings& settings,
                    bool held, float* output, unsigned frames)
        {
            const float length = settings.last - settings.first;
            const float scan_step = settings.speed / length;
            for (unsigned frame = 0; frame < frames; ++frame) {
                if (held && wait_ <= 0) {
                    spawn(settings, length);
                    wait_ += settings.interval;
                }
                if (held) --wait_;
                float sum = 0;
                if (active_) {
                    for (auto& grain : grains_) {
                        if (!grain.remaining) continue;
                        const unsigned i = std::min<unsigned>(grain.position, grain.last - 1);
                        const unsigned next = i + 1 < grain.last ? i + 1 : grain.first;
                        const auto pcm = source.pcm_;
                        const float sample = pcm[i] + (pcm[next] - pcm[i]) * (grain.position - i);
                        const unsigned phase = std::min(256u, grain.phase >> 16);
                        const float window = grain.window == 0 ? source.hann_[phase] :
                            grain.window == 1 ? 1 - std::abs(float(phase) / 128 - 1) : 1;
                        sum += sample * window;
                        grain.phase += grain.step;
                        grain.position += grain.rate;
                        if (grain.position >= grain.last) {
                            const float width = grain.last - grain.first;
                            grain.position -= width;
                            if (grain.position >= grain.last) {
                                grain.position = grain.first + std::fmod(grain.position - grain.first, width);
                            }
                        }
                        if (--grain.remaining == 0) --active_;
                    }
                }
                output[frame] = sum * settings.gain;
                scan_ += scan_step;
                if (scan_ < 0) scan_ += 1;
                else if (scan_ >= 1) scan_ -= 1;
            }
        }
    private:
        struct Grain {
            float position = 0, rate = 1;
            unsigned first = 0, last = 0, remaining = 0, phase = 0, step = 0;
            int window = 0;
        };

        void spawn(const Settings& settings, float length)
        {
            if (active_ == max_grains) return;
            auto it = std::find_if(grains_.begin(), grains_.end(), [](const Grain& g) { return !g.remaining; });
            random_ ^= random_ << 13;
            random_ ^= random_ >> 17;
            random_ ^= random_ << 5;
            const float random = (random_ >> 8) * (2.0f / 16777216) - 1;
            const float offset = std::clamp(scan_ * length + random * settings.spray, 0.0f, length - 1);
            *it = {settings.first + offset, settings.ratio, settings.first, settings.last,
                   settings.duration, 0, (256u << 16) / std::max(1u, settings.duration - 1), settings.window};
            ++active_;
        }

        std::array<Grain, max_grains> grains_{};
        unsigned active_ = 0;
        uint32_t random_ = 1;
        float scan_ = 0, wait_ = 0;
    };

private:
    static constexpr const char* windows[] = {"Hann", "Triangle", "Rect"};
    Parameter parameters_[parameter_count] = {
        {"Start", 0, 99, 1, 0, "%"}, {"End", 1, 100, 1, 100, "%"},
        {"Size", 5, 300, 1, 80, "ms"}, {"Density", 1, 60, 1, 20, "Hz"},
        {"Speed", -200, 200, 1, 100, "%"}, {"Spray", 0, 100, 1, 10, "%"},
        {"Window", 0, 2, 1, 0, "", windows}, {"Pitch", -24, 24, 1, 0, "st"},
        {"Level", 0, 100, 1, 100, "%"},
    };
    std::span<const int16_t> pcm_;
    std::array<float, 257> hann_{};
};
