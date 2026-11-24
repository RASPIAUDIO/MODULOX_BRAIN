#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Single owner on the audio core. Enabling RX resets the IDF DMA queue; stop
// before changing owners so a recorder never receives old live-monitor data.
template<unsigned Frames, unsigned QueueDepth = 4>
class AudioInputStream {
public:
    enum class Mode { off, live, recording };
    struct Read { size_t bytes; bool failed; };
    Mode mode() const { return mode_; }
    bool failed() const { return failed_; }

    template<class Enable, class Disable>
    bool select(Mode requested, Enable enable, Disable disable)
    {
        if (mode_ == requested) return true;
        if (mode_ != Mode::off && !disable()) return false;
        mode_ = Mode::off;
        if (requested != Mode::off && !enable()) return false;
        mode_ = requested;
        return true;
    }

    // Reader must be nonblocking. Bound work to the DMA queue depth, keeping
    // the newest complete block rather than adding latency after a stall.
    // An empty queue produces silence, never replay of the previous block.
    template<class Reader>
    const int16_t* latest(Reader read)
    {
        failed_ = false;
        if (mode_ != Mode::live) return nullptr;
        bool available = false;
        for (unsigned i = 0; i < QueueDepth; ++i) {
            const Read result = read(scratch_.data(), sizeof(scratch_));
            if (result.failed) { failed_ = true; return nullptr; }
            if (!result.bytes) break;
            if (result.bytes != sizeof(scratch_)) { failed_ = true; return nullptr; }
            samples_ = scratch_;
            available = true;
        }
        return available ? samples_.data() : nullptr;
    }
private:
    Mode mode_ = Mode::off;
    bool failed_ = false;
    std::array<int16_t, Frames * 2> samples_{}, scratch_{};
};
