#pragma once

#include "patch_plan.h"

// Display order only. DSP slots and the audio execution plan keep their identities.
inline constexpr std::array module_menu_order{
    oscillator, oscillator2, oscillator3, envelope, filter, mixer,
    lfo, lfo2, lfo3, env1, env2, midi, disto, delay,
    sampler, sampler2, sampler3, granulizer, clock1, clock2, arp,
    sample_hold, sample_hold2, random1, random2, gain_offset, slew,
    noise, sub, vca, ring_mod, wavefolder,
    pan, pan2, pan3, chorus, rotary, stereo_join, mono_down, stereo_mix, fm4, reverb, decimator, bitcrusher, compressor, tb303,
};
inline constexpr unsigned menu_module_slot(ModuleMask mask, unsigned position)
{
    for (const auto slot : module_menu_order)
        if ((mask & module_bit(slot)) && position-- == 0) return slot;
    return max_patch_modules;
}
inline constexpr bool valid_module_menu_order()
{
    ModuleMask seen = 0;
    for (const auto slot : module_menu_order) {
        if (seen & module_bit(slot)) return false;
        seen |= module_bit(slot);
    }
    return seen == (module_bit(max_patch_modules) - 1);
}
static_assert(module_menu_order.size() == max_patch_modules && valid_module_menu_order());

inline constexpr unsigned menu_visible_rows = 5;
inline constexpr unsigned menu_top = 50;
inline constexpr unsigned menu_row_height = 38;
