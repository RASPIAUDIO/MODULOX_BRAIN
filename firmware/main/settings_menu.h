#pragma once

#include "patch_settings.h"

// Hardware configuration is a separate page, never a saved patch parameter.
// Keep the existing Save index and all MIDI Learn/JSON indices unchanged.
inline constexpr unsigned settings_save_entry = patch_setting_count;
inline constexpr unsigned settings_speaker_entry = settings_save_entry + 1;
inline constexpr unsigned settings_entry_count = settings_speaker_entry + 1;
inline constexpr unsigned speaker_field_count = 2;
inline constexpr bool settings_is_action(unsigned position)
{ return position >= settings_save_entry && position < settings_entry_count; }

inline constexpr unsigned settings_next(unsigned position, int direction)
{
    return (int(position) + direction % int(settings_entry_count) + int(settings_entry_count)) % settings_entry_count;
}
