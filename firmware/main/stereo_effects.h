#pragma once

#include "effects.h"
#include "osc.h"

class AudioMemory {
public:
    bool prepare(unsigned samples)
    {
        if (data_) return true;
        const unsigned bytes = samples * sizeof(int16_t);
#ifdef ESP_PLATFORM
        auto* memory = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > bytes + 48 * 1024
            ? static_cast<int16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) : nullptr;
        if (!memory) memory = static_cast<int16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
        auto* memory = static_cast<int16_t*>(std::malloc(bytes));
#endif
        data_.reset(memory);
        return bool(data_);
    }
    int16_t* get() { return data_.get(); }
    bool allocated() const { return bool(data_); }
    static int16_t pcm(float value) { return static_cast<int16_t>(std::clamp(value, -32768.f, 32767.f)); }
    static float tap(const int16_t* data, unsigned capacity, unsigned position, unsigned filled, float distance)
    {
        const unsigned whole = unsigned(distance);
        unsigned at = position + capacity - whole;
        if (at >= capacity) at -= capacity;
        const float a = filled >= whole ? data[at] : 0;
        const float b = filled > whole ? data[at ? at - 1 : capacity - 1] : 0;
        return a + (b - a) * (distance - whole);
    }
private:
    std::unique_ptr<int16_t[], decltype(&std::free)> data_{nullptr, std::free};
};

class Pan {
public:
    struct Gains { float left, right; };
    ModuleParameters controls(const char* name) { return {name, parameters_}; }
    Gains gains(const Modulation* position, const Modulation* level) const
    {
        const float at = effect_parameter(parameters_[0], position) * 0.01f;
        const float gain = effect_parameter(parameters_[1], level) * 0.01f;
        // Equal-power law evaluated once per block, never per audio sample.
        const auto phase = uint32_t((at + 1) * 536870912.0f);
        return {Osc::sample(0, phase + 0x40000000u) * gain, Osc::sample(0, phase) * gain};
    }
    static void process(const float* input, float* left, float* right, unsigned frames, Gains target, Gains& state)
    {
        const Gains step{(target.left - state.left) / frames, (target.right - state.right) / frames};
        for (unsigned i = 0; i < frames; ++i) {
            state.left += step.left; state.right += step.right;
            left[i] = input[i] * state.left; right[i] = input[i] * state.right;
        }
        state = target;
    }
private:
    Parameter parameters_[2] = {{"Pan", -100, 100, 1, 0, "%"}, {"Level", 0, 100, 1, 100, "%"}};
};

class Chorus {
public:
    ModuleParameters controls() { return {"Chorus", parameters_}; }
    unsigned allocated_bytes() const { return memory_.allocated() ? 2 * capacity * sizeof(int16_t) : 0; }
    bool prepare() { return memory_.prepare(2 * capacity); }
    void reset() { position_ = filled_ = phase_ = 0; depth_ = wet_ = 0; }
    void process(float* left, float* right, unsigned frames, const Modulation* rate, const Modulation* depth, const Modulation* mix)
    {
        const auto increment = uint32_t(effect_parameter(parameters_[0], rate) * (4294967296.0f / (100 * 44100)));
        const float target_depth = effect_parameter(parameters_[1], depth) * 4.41f;
        const float target_wet = effect_parameter(parameters_[2], mix) * 0.01f;
        const float depth_step = (target_depth - depth_) / frames, wet_step = (target_wet - wet_) / frames;
        auto* l = memory_.get(); auto* r = l + capacity;
        for (unsigned i = 0; i < frames; ++i) {
            depth_ += depth_step; wet_ += wet_step;
            const float dl = AudioMemory::tap(l, capacity, position_, filled_, 529.2f + depth_ * Osc::sample(0, phase_));
            const float dr = AudioMemory::tap(r, capacity, position_, filled_, 529.2f + depth_ * Osc::sample(0, phase_ + 0x40000000u));
            l[position_] = AudioMemory::pcm(left[i]); r[position_] = AudioMemory::pcm(right[i]);
            left[i] += (dl - left[i]) * wet_; right[i] += (dr - right[i]) * wet_;
            phase_ += increment;
            if (++position_ == capacity) position_ = 0;
            filled_ += filled_ < capacity;
        }
        depth_ = target_depth; wet_ = target_wet;
    }
private:
    static constexpr unsigned capacity = 2048;
    Parameter parameters_[3] = {{"Rate", 5, 800, 1, 80, "x0.01Hz"},
        {"Depth", 0, 100, 1, 50, "%"}, {"Mix", 0, 100, 1, 30, "%"}};
    AudioMemory memory_;
    unsigned position_ = 0, filled_ = 0;
    uint32_t phase_ = 0;
    float depth_ = 0, wet_ = 0;
};

class Rotary {
public:
    ModuleParameters controls() { return {"Rotary", parameters_}; }
    unsigned allocated_bytes() const { return memory_.allocated() ? 2 * capacity * sizeof(int16_t) : 0; }
    bool prepare() { return memory_.prepare(2 * capacity); }
    void reset() { position_ = filled_ = horn_phase_ = bass_phase_ = 0; low_ = speed_ = wet_ = 0; }
    void process(float* left, float* right, unsigned frames, const Modulation* speed, const Modulation* depth, const Modulation* mix)
    {
        const bool fast = effect_parameter(parameters_[0], speed) >= 0.5f;
        const float target_speed = parameters_[fast ? 2 : 1].get() * 0.01f;
        const float alpha = 1 / (1 + parameters_[3].get() * 44.1f);
        const float amount = effect_parameter(parameters_[4], depth) * 0.01f;
        const float target_wet = effect_parameter(parameters_[5], mix) * 0.01f;
        const float wet_step = (target_wet - wet_) / frames;
        auto* horn = memory_.get(); auto* bass = horn + capacity;
        for (unsigned i = 0; i < frames; ++i) {
            speed_ += (target_speed - speed_) * alpha;
            wet_ += wet_step;
            const float mono = (left[i] + right[i]) * 0.5f;
            low_ += (mono - low_) * 0.107745f; // 800 Hz crossover at 44.1 kHz.
            horn[position_] = AudioMemory::pcm(mono - low_); bass[position_] = AudioMemory::pcm(low_);
            const float h = Osc::sample(0, horn_phase_), b = Osc::sample(0, bass_phase_);
            const float hl = AudioMemory::tap(horn, capacity, position_, filled_, 66 + amount * 44 * h);
            const float hr = AudioMemory::tap(horn, capacity, position_, filled_, 66 - amount * 44 * h);
            const float bl = AudioMemory::tap(bass, capacity, position_, filled_, 88 + amount * 22 * b);
            const float br = AudioMemory::tap(bass, capacity, position_, filled_, 88 - amount * 22 * b);
            const float out_l = hl * (0.75f + 0.25f * amount * h) + bl * (0.85f + 0.15f * amount * b);
            const float out_r = hr * (0.75f - 0.25f * amount * h) + br * (0.85f - 0.15f * amount * b);
            left[i] += (out_l - left[i]) * wet_; right[i] += (out_r - right[i]) * wet_;
            const auto step = uint32_t(speed_ * (4294967296.0f / 44100));
            horn_phase_ += step; bass_phase_ += uint32_t(step * 0.72f);
            if (++position_ == capacity) position_ = 0;
            filled_ += filled_ < capacity;
        }
        wet_ = target_wet;
    }
private:
    static constexpr unsigned capacity = 512;
    static constexpr const char* speeds[] = {"Slow", "Fast"};
    Parameter parameters_[6] = {{"Speed", 0, 1, 1, 0, "", speeds},
        {"Slow", 10, 300, 1, 80, "x0.01Hz"}, {"Fast", 100, 1200, 1, 650, "x0.01Hz"},
        {"Transition", 50, 5000, 50, 1200, "ms"}, {"Depth", 0, 100, 1, 70, "%"}, {"Mix", 0, 100, 1, 100, "%"}};
    AudioMemory memory_;
    unsigned position_ = 0, filled_ = 0;
    uint32_t horn_phase_ = 0, bass_phase_ = 0;
    float low_ = 0, speed_ = 0, wet_ = 0;
};
