#pragma once

#include "display.h"
#include "parameter.h"
#include "wm8960_speaker.h"

#include <cstdio>

// One renderer for the LCD, USB mirror and native host preview/tests.
inline bool draw_speaker_panel(unsigned selected, ModuleParameters controls, SpeakerStatus status, bool force = false)
{
    const uint32_t key = selected | (unsigned(controls.parameters[0].get()) << 1) |
        (unsigned(controls.parameters[1].get() + 60) << 3) | (unsigned(status.output) << 9) |
        (status.gain << 11) | (unsigned(status.fault) << 18) | (unsigned(status.available) << 19);
    static uint32_t previous = ~0u;
    if (!force && previous == key) return false;
    previous = key;
    constexpr uint16_t white = 0xffff, black = 0, tile = 0x39c7, accent = 0x3fff;
    const auto& parameter = controls.parameters[selected];
    display::heading("Speaker");
    display::text(0, 52, 280, 38, parameter.name, 2, accent, black);
    char value[24]{};
    if (selected == 0) std::snprintf(value, sizeof(value), "%s", parameter.choices[parameter.get()]);
    else std::snprintf(value, sizeof(value), "%d dB", parameter.get());
    display::text(0, 90, 280, 55, value, 3, white, black);
    char state[48]{};
    if (!status.available) std::snprintf(state, sizeof(state), "Speaker control unavailable");
    else if (status.fault) std::snprintf(state, sizeof(state), "I2C error: output not confirmed");
    else if (status.output == SpeakerOutput::off) std::snprintf(state, sizeof(state), "Amplifier off");
    else std::snprintf(state, sizeof(state), "%s | actual %d dB",
        controls.parameters[0].get() == int(status.output) && status.gain == unsigned(121 + controls.parameters[1].get())
            ? "Active" : "Ramping", int(status.gain) - 121);
    display::text(0, 145, 280, 23, state, 1, white, black);
    display::text(0, 168, 280, 22, status.fault && status.available ? "Center: retry Off" : "Center: back | Off at boot", 1, white, black);
    for (unsigned i = 0; i < 2; ++i)
        display::button(i * 140, 190, 140, 50, controls.parameters[i].name, 2,
            i == selected ? black : white, i == selected ? accent : tile);
    return true;
}
