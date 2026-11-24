#pragma once

#include "lfo.h"

#include <cmath>
#include <memory>
#include <new>
#include <cstdlib>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

inline float effect_parameter(const Parameter& parameter, const Modulation* modulation)
{
    const float value = parameter.get();
    return modulation ? modulation->apply(value, parameter.minimum, parameter.maximum,
                                           parameter.maximum - parameter.minimum) : value;
}

class Distortion {
public:
    ModuleParameters controls() { return {"Disto", params_}; }
    void begin_block(const Modulation* mode = nullptr, const Modulation* drive = nullptr,
                     const Modulation* mix = nullptr)
    {
        mode_ = effect_parameter(params_[0], mode) >= 0.5f;
        gain_ = 1.0f + 0.15f * effect_parameter(params_[1], drive);
        wet_ = 0.01f * effect_parameter(params_[2], mix);
    }
    float process(float input) const
    {
        const float drive = input * gain_;
        const float shaped = mode_ == 0 ? drive / (1.0f + std::abs(drive))
                                         : std::clamp(drive, -1.0f, 1.0f);
        return input + (shaped - input) * wet_;
    }
private:
    static constexpr const char* modes[] = {"Soft", "Hard"};
    Parameter params_[3] = {{"Mode",0,1,1,0,"",modes}, {"Drive",0,100,1,25,"%"},
                            {"Mix",0,100,1,100,"%"}};
    int mode_ = 0;
    float gain_ = 1;
    float wet_ = 0;
};

class Delay {
public:
    static constexpr unsigned max_time = 2000;
    static constexpr unsigned capacity = 88200;
    ModuleParameters controls() { return {"Delay", params_}; }
    bool stereo_mode() const { return params_[5].get() != 0; }
    unsigned allocated_bytes() const { return (bool(buffer_) + bool(right_)) * capacity * sizeof(int16_t); }
    bool prepare()
    {
        if (!buffer_) {
#ifdef ESP_PLATFORM
            auto* data = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > sizeof(int16_t) * capacity + 48 * 1024
                ? static_cast<int16_t*>(heap_caps_malloc(sizeof(int16_t) * capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) : nullptr;
            if (!data) data = static_cast<int16_t*>(heap_caps_malloc(sizeof(int16_t) * capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
            auto* data = static_cast<int16_t*>(std::malloc(sizeof(int16_t) * capacity));
#endif
            buffer_.reset(data);
        }
        if (buffer_ && !right_) {
#ifdef ESP_PLATFORM
            auto* data = static_cast<int16_t*>(heap_caps_malloc(sizeof(int16_t) * capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            if (!data && heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) > sizeof(int16_t) * capacity + 48 * 1024)
                data = static_cast<int16_t*>(heap_caps_malloc(sizeof(int16_t) * capacity, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
            auto* data = static_cast<int16_t*>(std::malloc(sizeof(int16_t) * capacity));
#endif
            right_.reset(data);
        }
        return bool(buffer_) && bool(right_);
    }
    void reset()
    {
        position_ = filled_ = 0;
        previous_distance_ = 0;
        previous_right_ = 0;
        previous_mode_ = -1;
    }
    void process_stereo(float* left, float* right, unsigned frames,
                        const Modulation* time, const Modulation* time_r,
                        const Modulation* feedback_mod, const Modulation* mix, float bpm)
    {
        const int mode = params_[5].get();
        if (mode != previous_mode_) { position_ = filled_ = 0; previous_distance_ = previous_right_ = 0; previous_mode_ = mode; }
        const float feedback = effect_parameter(params_[1], feedback_mod) * 0.01f;
        const float wet = effect_parameter(params_[2], mix) * 0.01f;
        const bool sync = params_[3].get(), link = params_[8].get();
        const float ms_l = sync ? 60'000 / bpm * division_beats[params_[4].get()] : effect_parameter(params_[0], time);
        const float ms_r = link ? ms_l : sync ? 60'000 / bpm * division_beats[params_[7].get()] : effect_parameter(params_[6], time_r);
        const float target_l = std::clamp(ms_l * 44.1f, 44.1f, float(capacity));
        const float target_r = std::clamp(ms_r * 44.1f, 44.1f, float(capacity));
        float distance_l = previous_distance_ ? previous_distance_ : target_l;
        float distance_r = previous_right_ ? previous_right_ : target_r;
        const float step_l = (target_l - distance_l) / frames, step_r = (target_r - distance_r) / frames;
        for (unsigned i = 0; i < frames; ++i) {
            const float dl = tap(buffer_.get(), distance_l);
            const float dr = mode ? tap(right_.get(), distance_r) : dl;
            const float in_l = left[i], in_r = right[i];
            const float send_l = mode == 1 ? in_l : (in_l + in_r) * 0.5f;
            const float send_r = mode == 1 ? in_r : 0;
            buffer_[position_] = pcm(send_l + (mode == 2 ? dr : dl) * feedback);
            right_[position_] = pcm(send_r + (mode == 2 ? dl : dr) * feedback);
            left[i] = in_l + (dl - in_l) * wet;
            right[i] = in_r + (dr - in_r) * wet;
            if (++position_ == capacity) position_ = 0;
            filled_ += filled_ < capacity;
            distance_l += step_l; distance_r += step_r;
        }
        previous_distance_ = target_l; previous_right_ = target_r;
    }
    void process(float* signal, unsigned frames, const Modulation* time = nullptr,
                 const Modulation* feedback_mod = nullptr, const Modulation* mix = nullptr, float bpm = 120)
    {
        const float feedback = effect_parameter(params_[1], feedback_mod) * 0.01f;
        const float wet = effect_parameter(params_[2], mix) * 0.01f;
        if (time || params_[3].get()) {
            const float ms = params_[3].get() ? std::min<float>(max_time, 60'000 / bpm * division_beats[params_[4].get()])
                : effect_parameter(params_[0], time);
            const float distance = std::min(float(capacity), ms * 44.1f);
            process_modulated_time(signal, frames, distance, feedback, wet);
            return;
        }
        const unsigned distance = unsigned(params_[0].get()) * 441 / 10;
        unsigned read = (position_ + capacity - distance) % capacity;
        for (unsigned i = 0; i < frames; ++i) {
            const float delayed = filled_ >= distance ? buffer_[read] : 0;
            mix_sample(signal[i], delayed, feedback, wet);
            if (++read == capacity) read = 0;
        }
    }
private:
    static int16_t pcm(float value) { return static_cast<int16_t>(std::clamp(value, -32768.f, 32767.f)); }
    float tap(const int16_t* data, float distance) const
    {
        distance = std::clamp(distance, 1.f, float(capacity));
        const unsigned whole = unsigned(distance);
        unsigned at = position_ + capacity - whole;
        if (at >= capacity) at -= capacity;
        const float recent = filled_ >= whole ? data[at] : 0;
        const float older = filled_ > whole ? data[at ? at - 1 : capacity - 1] : 0;
        return recent + (older - recent) * (distance - whole);
    }
    void mix_sample(float& signal, float delayed, float feedback, float wet)
    {
        buffer_[position_] = static_cast<int16_t>(std::clamp(signal + delayed * feedback,
                                                             -32768.0f, 32767.0f));
        signal += (delayed - signal) * wet;
        if (++position_ == capacity) position_ = 0;
        filled_ += filled_ < capacity;
    }

    void process_modulated_time(float* signal, unsigned frames, float target, float feedback, float wet)
    {
        // Ramp the fractional read position across the block to avoid jumps between taps.
        float distance = previous_distance_ != 0 ? previous_distance_ : target;
        const float step = (target - distance) / frames;
        for (unsigned i = 0; i < frames; ++i) {
            distance = std::clamp(distance, 1.f, float(capacity));
            const unsigned whole = static_cast<unsigned>(distance);
            unsigned read = position_ + capacity - whole;
            if (read >= capacity) read -= capacity;
            const float recent = filled_ >= whole ? buffer_[read] : 0;
            const unsigned older_index = read ? read - 1 : capacity - 1;
            const float older = filled_ > whole ? buffer_[older_index] : 0;
            const float delayed = recent + (older - recent) * (distance - whole);
            mix_sample(signal[i], delayed, feedback, wet);
            distance += step;
        }
        previous_distance_ = target;
    }

    static constexpr const char* sync_modes[] = {"Free", "Sync"};
    static constexpr const char* modes[] = {"Mono", "Stereo", "Ping-pong"};
    static constexpr const char* switches[] = {"Off", "On"};
    Parameter params_[9] = {{"Time",1,max_time,1,220,"ms",{},20},
                            {"Feedback",0,95,1,35,"%"}, {"Mix",0,100,1,30,"%"},
                            {"Sync",0,1,1,0,"",sync_modes}, {"Division",0,9,1,4,"",division_names},
                            {"Mode",0,2,1,0,"",modes}, {"Time R",1,max_time,1,330,"ms",{},20},
                            {"Division R",0,9,1,4,"",division_names}, {"Link",0,1,1,1,"",switches}};
    std::unique_ptr<int16_t[], decltype(&std::free)> buffer_{nullptr, std::free};
    std::unique_ptr<int16_t[], decltype(&std::free)> right_{nullptr, std::free};
    unsigned position_ = 0;
    unsigned filled_ = 0;
    float previous_distance_ = 0;
    float previous_right_ = 0;
    int previous_mode_ = -1;
};
