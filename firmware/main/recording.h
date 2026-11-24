#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <span>

enum class RecordingState : unsigned { idle, armed, recording, ready, processing, saving, saved, no_memory, input_error, save_error, storage_busy, no_storage };

// One producer (audio core), one consumer (storage task). Publish ready only after RX has stopped.
class Recording {
public:
    static constexpr unsigned sample_rate = 44100;
    static constexpr unsigned max_seconds = 5;
    static constexpr unsigned max_frames = sample_rate * max_seconds;
    static constexpr unsigned trim_start_ms = 30, trim_end_ms = 170, fade_ms = 2;
    // Runs once on the storage core, after RX stops. The returned PCM stays at the buffer base.
    static unsigned prepare(std::span<int16_t> pcm)
    {
        if (pcm.size() < 2) return pcm.size();
        constexpr unsigned wanted_start = sample_rate * trim_start_ms / 1000;
        constexpr unsigned wanted_end = sample_rate * trim_end_ms / 1000;
        // Preserve at least half of a very short take instead of trimming it away.
        const unsigned trim = std::min<unsigned>(wanted_start + wanted_end, (pcm.size() - 2) / 2);
        const unsigned first = trim * wanted_start / (wanted_start + wanted_end);
        const unsigned frames = pcm.size() - trim;
        std::memmove(pcm.data(), pcm.data() + first, frames * sizeof(int16_t));
        const unsigned fade = std::min<unsigned>(sample_rate * fade_ms / 1000, frames / 2);
        for (unsigned i = 0; i < fade; ++i) {
            const unsigned denominator = std::max(1u, fade - 1);
            pcm[i] = int32_t(pcm[i]) * int(i) / int(denominator);
            pcm[frames - 1 - i] = int32_t(pcm[frames - 1 - i]) * int(i) / int(denominator);
        }
        int peak = 0;
        for (unsigned i = 0; i < frames; ++i) peak = std::max(peak, std::abs(int(pcm[i])));
        if (peak) for (unsigned i = 0; i < frames; ++i) {
            const int scaled = int(pcm[i]) * 32767;
            pcm[i] = (scaled + (scaled < 0 ? -peak / 2 : peak / 2)) / peak;
        }
        return frames;
    }
    RecordingState state() const { return state_.load(std::memory_order_acquire); }
    unsigned frames() const { return frames_.load(std::memory_order_relaxed); }
    bool busy() const { return state() >= RecordingState::armed && state() <= RecordingState::saving; }
    void arm(std::span<int16_t> destination)
    {
        data_ = destination.first(std::min<unsigned>(destination.size(), max_frames));
        frames_.store(0, std::memory_order_relaxed);
        stop_.store(false, std::memory_order_relaxed);
        set(RecordingState::armed);
    }
    void set(RecordingState state) { state_.store(state, std::memory_order_release); }
    void stop() { stop_.store(true, std::memory_order_release); }
    bool stopped() const { return stop_.load(std::memory_order_acquire); }
    bool capture(std::span<const int16_t> stereo)
    {
        const unsigned first = frames();
        const unsigned count = std::min<unsigned>(stereo.size() / 2, data_.size() - first);
        for (unsigned i = 0; i < count; ++i) data_[first + i] = stereo[2 * i];
        frames_.store(first + count, std::memory_order_release);
        return stopped() || first + count == data_.size();
    }
private:
    std::span<int16_t> data_;
    std::atomic<RecordingState> state_{RecordingState::idle};
    std::atomic<unsigned> frames_{0};
    std::atomic<bool> stop_{false};
};

enum class SampleDeleteStatus : unsigned { none, deleted, in_use, busy, changed, error };

struct SampleLibraryStatus {
    RecordingState recording = RecordingState::idle;
    unsigned milliseconds = 0, duration_ms = 0, count = 0, index = 0, revision = 0;
};
