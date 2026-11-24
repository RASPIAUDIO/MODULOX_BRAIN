#pragma once

#include <algorithm>
#include <atomic>
#include <span>

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

inline constexpr unsigned max_module_parameters = 12;

struct Parameter {
    const char* name;
    int minimum;
    int maximum;
    int step;
    int initial;
    const char* unit = "";
    std::span<const char* const> choices = {};
    int step_divisor;

    int get() const { return value_.load(std::memory_order_relaxed); }
    void set(int value) { value_.store(std::clamp(value, minimum, maximum), std::memory_order_relaxed); }

    Parameter(const char* label, int min, int max, int increment, int default_value,
              const char* suffix = "", std::span<const char* const> options = {},
              int proportional_step_divisor = 0)
        : name(label), minimum(min), maximum(max), step(increment), initial(default_value),
          unit(suffix), choices(options), step_divisor(proportional_step_divisor),
          value_(default_value) {}

private:
    std::atomic<int> value_;
};

// With the ESP-IDF PSRAM workaround, the compiler dispatches atomics by
// address. Parameter storage stays in internal RAM and uses the hardware path.
#if !defined(CONFIG_STDATOMIC_S32C1I_SPIRAM_WORKAROUND)
static_assert(std::atomic<int>::is_always_lock_free);
#endif

struct ParameterPage { const char* name; unsigned first, count; };

struct ModuleParameters {
    const char* name;
    std::span<Parameter> parameters;
    void (*on_change)(void*) = nullptr;
    void* context = nullptr;
    const std::atomic<unsigned>* hidden_parameters = nullptr;
    std::span<const ParameterPage> pages{};
    std::span<const unsigned> menu_order{};

    bool visible(unsigned index) const
    {
        return !hidden_parameters || !(hidden_parameters->load(std::memory_order_relaxed) & (1u << index));
    }

    unsigned visible_count() const
    {
        unsigned count = 0;
        for (unsigned i = 0; i < parameters.size(); ++i) count += visible(i);
        return count;
    }

    // Menu positions can change; stored parameter and MIDI Learn indices stay stable.
    unsigned parameter_at(unsigned position) const
    {
        if (!menu_order.empty()) {
            for (const unsigned index : menu_order)
                if (visible(index) && position-- == 0) return index;
            return parameters.size();
        }
        for (unsigned i = 0; i < parameters.size(); ++i) {
            if (visible(i) && position-- == 0) return i;
        }
        return parameters.size();
    }

    unsigned parameter_position(unsigned index) const
    {
        for (unsigned position = 0; position < visible_count(); ++position)
            if (parameter_at(position) == index) return position;
        return parameters.size();
    }

    void set(int index, int value) const
    {
        auto& parameter = parameters[index];
        const int previous = parameter.get();
        parameter.set(value);
        if (on_change && previous != parameter.get()) {
            on_change(context);
        }
    }
};
