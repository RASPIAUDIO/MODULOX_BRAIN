#pragma once
#include <atomic>

// Single display consumer and single audio producer. No locks or allocation.
// Audio only writes the payload after a request; GUI only reads a ready payload.
template<class T> class DisplayMailbox {
public:
    bool request() {
        if (state_.load(std::memory_order_acquire) == requested) return false;
        state_.store(requested, std::memory_order_release);
        return true;
    }
    void cancel() { state_.store(idle, std::memory_order_release); } // Only when queuing failed.
    bool wanted() const { return state_.load(std::memory_order_acquire) == requested; }
    void publish(const T& value) { value_ = value; state_.store(ready, std::memory_order_release); }
    bool read(T& value) {
        if (state_.load(std::memory_order_acquire) != ready) return false;
        value = value_;
        state_.store(idle, std::memory_order_release);
        return true;
    }
private:
    enum : unsigned { idle, requested, ready };
    T value_{};
    std::atomic<unsigned> state_{idle};
    // Only aligned loads/stores are used; no compare/exchange or fetch operations.
    // The Xtensa toolchain does not advertise all std::atomic operations as lock-free.
};
