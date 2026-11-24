#pragma once

#include "parameter.h"
#include "transport.h"

inline constexpr unsigned max_voices = 8;
inline constexpr unsigned patch_setting_count = 12;
enum PatchSetting { polyphony, master_volume, midi_channel, tempo_bpm, clock_source, transport_run, sync_ppqn, clock_loss,
                    play_mode, glide_mode, glide_time, legato_retrigger };

inline constexpr const char* midi_channels[] = {
    "Omni", "1", "2", "3", "4", "5", "6", "7", "8",
    "9", "10", "11", "12", "13", "14", "15", "16",
};

struct PatchSettings {
    static constexpr const char* clock_sources[] = {"Internal", "USB", "DIN", "Sync jack"};
    static constexpr const char* switches[] = {"Off", "On"};
    static constexpr const char* loss_modes[] = {"Stop", "Hold tempo"};
    static constexpr const char* play_modes[] = {"Poly", "Mono"};
    static constexpr const char* glide_modes[] = {"Off", "Always", "Legato"};
    Parameter parameters[patch_setting_count] = {
        {"Voices", 1, max_voices, 1, 1},
        {"Volume", 0, 100, 1, 77, "%"},
        {"MIDI Ch", 0, 16, 1, 0, "", midi_channels},
        {"BPM", 20, 300, 1, 120},
        {"Clock", 0, 3, 1, 0, "", clock_sources},
        {"Run", 0, 1, 1, 1, "", switches},
        {"Jack PPQN", 0, 3, 1, 1, "", sync_ppqn_names},
        {"Clock loss", 0, 1, 1, 0, "", loss_modes},
        {"Play mode", 0, 1, 1, 0, "", play_modes},
        {"Glide", 0, 2, 1, 0, "", glide_modes},
        {"Glide time", 0, 2000, 5, 100, "ms"},
        {"Retrigger", 0, 1, 1, 0, "", switches},
    };

    ModuleParameters controls() { return {"Settings", parameters}; }
};
