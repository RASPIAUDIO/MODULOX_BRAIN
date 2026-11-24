#pragma once

#include "effects.h"
#include <array>

// Eight-line orthogonal feedback delay network at 44.1 kHz. All large
// buffers live in PSRAM; no allocation, clearing or transcendental per sample.
class Reverb {
public:
    static constexpr unsigned predelay_capacity = 4412;
    static constexpr std::array<unsigned, 8> lengths{1493, 1601, 1747, 1867, 1999, 2131, 2281, 2411};
    static constexpr std::array<unsigned, 4> diffusion_lengths{113, 173, 127, 181};
    static constexpr unsigned network_samples = [] { unsigned n = 0; for (auto size : lengths) n += size; return n; }();
    static constexpr unsigned diffusion_samples = [] { unsigned n = 0; for (auto size : diffusion_lengths) n += size; return n; }();
    static constexpr unsigned buffer_samples = network_samples + diffusion_samples + 2 * predelay_capacity;
    static constexpr unsigned buffer_bytes = buffer_samples * sizeof(float);
    ModuleParameters controls() { return {"Reverb", parameters_}; }
    unsigned allocated_bytes() const { return memory_ ? buffer_bytes : 0; }
    bool prepare()
    {
        if (memory_) return true;
#ifdef ESP_PLATFORM
        auto* data = static_cast<float*>(heap_caps_malloc(buffer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
        auto* data = static_cast<float*>(std::malloc(buffer_bytes));
#endif
        memory_.reset(data);
        return bool(memory_);
    }
    void reset()
    {
        positions_.fill(0); filled_.fill(0); low_.fill(0);
        diffusion_positions_.fill(0); diffusion_filled_.fill(0);
        predelay_position_ = predelay_filled_ = 0;
        decay_ms_ = -1; initialized_ = false;
    }
    void process(float* left, float* right, unsigned frames,
                 const Modulation* decay = nullptr, const Modulation* damping = nullptr,
                 const Modulation* predelay = nullptr, const Modulation* width = nullptr,
                 const Modulation* mix = nullptr)
    {
        if (!memory_ || !frames) return;
        const float ms = effect_parameter(parameters_[0], decay);
        if (ms != decay_ms_) {
            decay_ms_ = ms;
            for (unsigned j = 0; j < 8; ++j)
                target_feedback_[j] = std::pow(0.001f, lengths[j] / (44.1f * ms));
        }
        const std::array<float, 4> target{
            0.05f + 0.9f * effect_parameter(parameters_[1], damping) * 0.01f,
            effect_parameter(parameters_[2], predelay) * 44.1f,
            effect_parameter(parameters_[3], width) * 0.01f,
            effect_parameter(parameters_[4], mix) * 0.01f};
        if (!initialized_) { smooth_ = target; feedback_ = target_feedback_; initialized_ = true; }
        const bool ramp = smooth_ != target || feedback_ != target_feedback_;
        std::array<float, 8> feedback_step;
        for (unsigned j = 0; j < 8; ++j) feedback_step[j] = (target_feedback_[j] - feedback_[j]) / frames;
        std::array<float, 4> step;
        for (unsigned j = 0; j < 4; ++j) step[j] = (target[j] - smooth_[j]) / frames;
        auto* pre_l = memory_.get() + network_samples + diffusion_samples;
        auto* pre_r = pre_l + predelay_capacity;
        for (unsigned i = 0; i < frames; ++i) {
            if (ramp) {
                for (unsigned j = 0; j < 4; ++j) smooth_[j] += step[j];
                for (unsigned j = 0; j < 8; ++j) feedback_[j] += feedback_step[j];
            }
            const float dry_l = left[i], dry_r = right[i];
            pre_l[predelay_position_] = dry_l; pre_r[predelay_position_] = dry_r;
            // Both channels share the same read position. A bounded wrap avoids
            // integer division in this sample loop, even during modulation.
            const float distance = std::clamp(smooth_[1], 0.f, 4410.f);
            const unsigned whole = unsigned(distance);
            unsigned at = predelay_position_ + predelay_capacity - whole;
            if (at >= predelay_capacity) at -= predelay_capacity;
            const auto pre = [&](const float* data) {
                const float a = predelay_filled_ >= whole ? data[at] : 0;
                const float b = predelay_filled_ > whole ? data[at ? at - 1 : predelay_capacity - 1] : 0;
                return a + (b - a) * (distance - whole);
            };
            float input_l = pre(pre_l), input_r = pre(pre_r);
            if (++predelay_position_ == predelay_capacity) predelay_position_ = 0;
            if (predelay_filled_ < predelay_capacity) ++predelay_filled_;
            // Two unity-magnitude allpasses per input increase early echo density.
            unsigned offset = network_samples;
            for (unsigned j = 0; j < 4; ++j) {
                float& input = j < 2 ? input_l : input_r;
                float& cell = memory_.get()[offset + diffusion_positions_[j]];
                const float delayed = diffusion_filled_[j] == diffusion_lengths[j] ? cell : 0;
                const float out = delayed - 0.5f * input;
                cell = input + 0.5f * out; input = out;
                if (++diffusion_positions_[j] == diffusion_lengths[j]) diffusion_positions_[j] = 0;
                if (diffusion_filled_[j] < diffusion_lengths[j]) ++diffusion_filled_[j];
                offset += diffusion_lengths[j];
            }
            std::array<float, 8> taps;
            offset = 0;
            for (unsigned j = 0; j < 8; ++j) {
                const float value = filled_[j] == lengths[j] ? memory_.get()[offset + positions_[j]] : 0;
                low_[j] = value * (1 - smooth_[0]) + low_[j] * smooth_[0];
                taps[j] = low_[j]; offset += lengths[j];
            }
            const float wet_l = (taps[0] + taps[2] - taps[4] - taps[6]) * 0.5f;
            const float wet_r = (taps[1] + taps[3] - taps[5] - taps[7]) * 0.5f;
            // Normalized Hadamard matrix: energy-preserving feedback mixing.
            for (unsigned stride = 1; stride < 8; stride *= 2)
                for (unsigned base = 0; base < 8; base += 2 * stride)
                    for (unsigned j = base; j < base + stride; ++j) {
                        const float a = taps[j], b = taps[j + stride];
                        taps[j] = a + b; taps[j + stride] = a - b;
                    }
            offset = 0;
            for (unsigned j = 0; j < 8; ++j) {
                const float injection = (j & 1 ? input_r : input_l) * (j & 2 ? -0.25f : 0.25f);
                memory_.get()[offset + positions_[j]] = injection + taps[j] * (0.35355339f * feedback_[j]);
                if (++positions_[j] == lengths[j]) positions_[j] = 0;
                if (filled_[j] < lengths[j]) ++filled_[j];
                offset += lengths[j];
            }
            const float mid = (wet_l + wet_r) * 0.5f, side = (wet_l - wet_r) * (0.5f * smooth_[2]);
            left[i] = dry_l + (mid + side - dry_l) * smooth_[3];
            right[i] = dry_r + (mid - side - dry_r) * smooth_[3];
        }
        feedback_ = target_feedback_;
        smooth_ = target;
    }
private:
    Parameter parameters_[5] = {{"Decay", 200, 10000, 50, 2000, "ms"},
        {"Damping", 0, 100, 1, 40, "%"}, {"Pre-delay", 0, 100, 1, 10, "ms"},
        {"Width", 0, 100, 1, 100, "%"}, {"Mix", 0, 100, 1, 25, "%"}};
    std::unique_ptr<float[], decltype(&std::free)> memory_{nullptr, std::free};
    std::array<unsigned, 8> positions_{}, filled_{};
    std::array<float, 8> low_{}, feedback_{}, target_feedback_{};
    std::array<unsigned, 4> diffusion_positions_{}, diffusion_filled_{};
    std::array<float, 4> smooth_{};
    unsigned predelay_position_ = 0, predelay_filled_ = 0;
    float decay_ms_ = -1;
    bool initialized_ = false;
};
