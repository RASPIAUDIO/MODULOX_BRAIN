#include "wm8960_speaker.h"
#include "speaker_panel.h"
#include "settings_menu.h"
#include "display_painter.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

struct Codec {
    std::array<unsigned, 56> registers{};
    std::array<unsigned, 2> volumes{};
    unsigned attempts = 0, fail_at = 0;
    bool dead = false, clocks = false;
    Codec() {
        registers[0x02] = 0x68; registers[0x03] = 0x168;
        registers[0x19] = 0xfe; registers[0x1a] = Wm8960Speaker::power2_base;
        registers[0x31] = 0x37; registers[0x33] = 0x80;
    }
    bool write(uint8_t address, uint16_t value) {
        ++attempts;
        assert(value <= 0x1ff);
        // Speaker transactions must not reset the codec or modify clocks,
        // DAC/input routing, headphone volume, or microphone/ADC power.
        assert(address == 0x17 || address == 0x30 || address == 0x33 || address == 0x1a ||
               address == 0x31 || address == 0x28 || address == 0x29);
        if (dead || attempts == fail_at) return false;
        if (address == 0x31) {
            assert((value & 0x3f) == 0x37);
            if (value & 0xc0) { assert(clocks); assert(volumes[0] == 0 && volumes[1] == 0); }
        }
        if (address == 0x1a) assert((value & ~0x18) == Wm8960Speaker::power2_base);
        if (address == 0x33) assert(value == 0x80); // No AC/DC boost; reserved bits intact.
        if (address == 0x17) assert(value & 0x100);
        if (address == 0x30) assert(value & 2);
        registers[address] = value;
        if ((address == 0x28 || address == 0x29) && (value & 0x100)) {
            volumes = {registers[0x28] & 127, registers[0x29] & 127};
        }
        return true;
    }
    void preserved() const {
        assert(registers[0x02] == 0x68 && registers[0x03] == 0x168 && registers[0x19] == 0xfe);
    }
    void off() const {
        assert(registers[0x31] == 0x37 && registers[0x1a] == Wm8960Speaker::power2_base);
        assert(volumes[0] == 0 && volumes[1] == 0);
        preserved();
    }
};

static std::array<uint16_t, 280 * 240> frame;
static std::vector<std::string> labels;
namespace display {
void text(int x, int y, int w, int h, std::string_view label, int scale,
          uint16_t ink, uint16_t bg, TextStyle style) {
    assert(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= 280 && y + h <= 240);
    labels.emplace_back(label);
    TextRaster raster(w, h, label, scale, ink, bg, style);
    for (int row = 0; row < h; ++row) raster.row(row, {frame.data() + (y + row) * 280 + x, unsigned(w)});
}
void heading(std::string_view label) { text(0, 0, 280, 50, label, 3, 0xffff, 0x39c7, TextStyle::heading); }
}
static void image(const char* name) {
    FILE* file = std::fopen(name, "wb"); assert(file);
    std::fprintf(file, "P6\n280 240\n255\n");
    for (auto color : frame) {
        const unsigned char rgb[] = {uint8_t((color >> 11) * 255 / 31), uint8_t(((color >> 5) & 63) * 255 / 63), uint8_t((color & 31) * 255 / 31)};
        std::fwrite(rgb, 1, 3, file);
    }
    std::fclose(file);
}

int main() {
    static_assert(patch_setting_count == 12 && settings_save_entry == 12 && settings_speaker_entry == 13);
    static_assert(settings_is_action(settings_save_entry) && settings_is_action(settings_speaker_entry));
    static_assert(!settings_is_action(patch_setting_count - 1) && !settings_is_action(settings_entry_count));
    for (unsigned i = 0; i < settings_entry_count; ++i) {
        assert(settings_next(settings_next(i, 1), -1) == i);
        assert(settings_next(i, 14) == i);
    }
    assert(settings_next(0, -1) == settings_speaker_entry && settings_next(settings_speaker_entry, 1) == 0);

    Codec codec; Wm8960Speaker speaker;
    const auto write = [&](uint8_t r, uint16_t v) { return codec.write(r, v); };
    speaker.service({}, 0, write); assert(codec.attempts == 0); codec.off(); // Safe before clocks.
    codec.clocks = true;
    speaker.service({SpeakerOutput::left, -30}, 1, write);
    assert(!speaker.status().fault && speaker.status().gain == 48 && codec.attempts == 11);
    assert(codec.registers[0x31] == 0x77 && codec.registers[0x1a] == 0x1f3);
    assert(codec.volumes[0] == 48 && codec.volumes[1] == 0);
    for (unsigned i = 0; i < 100; ++i) {
        const auto before = speaker.status().gain;
        const auto attempts = codec.attempts;
        speaker.service({SpeakerOutput::left, -30}, 1, write);
        assert(speaker.status().gain <= before + 1 && codec.attempts - attempts <= 2);
    }
    assert(speaker.status().gain == 91 && codec.volumes[0] == 91);
    const auto settled = codec.attempts;
    for (unsigned i = 0; i < 1000; ++i) speaker.service({SpeakerOutput::left, -30}, 1, write);
    assert(codec.attempts == settled); // No I2C traffic at a stable setting.
    for (unsigned i = 0; i < 100; ++i) speaker.service({SpeakerOutput::left, 100}, 2, write);
    assert(speaker.status().gain == 121); // Clamped to 0 dB, never +6 dB.
    speaker.service({SpeakerOutput::right, -60}, 3, write);
    assert(codec.registers[0x31] == 0xb7 && codec.registers[0x1a] == 0x1eb);
    assert(codec.volumes[0] == 0 && codec.volumes[1] == 48);
    for (unsigned i = 0; i < 100; ++i) speaker.service({SpeakerOutput::right, -100}, 4, write);
    assert(speaker.status().gain == 61);
    speaker.service({SpeakerOutput::stereo, -30}, 5, write);
    assert(codec.registers[0x31] == 0xf7 && codec.registers[0x1a] == 0x1fb);
    assert(codec.volumes[0] == 48 && codec.volumes[1] == 48);
    speaker.service({}, 6, write); codec.off();
    speaker.service({SpeakerOutput(99), 0}, 7, write); codec.off();

    // Inject a NACK/timeout at EVERY enable write, including partial stereo
    // volume commits. Cleanup is bounded and no later On request retries it.
    for (unsigned failure = 1; failure <= 11; ++failure) {
        Codec bus; bus.clocks = true; bus.fail_at = failure;
        Wm8960Speaker controller;
        const auto send = [&](uint8_t r, uint16_t v) { return bus.write(r, v); };
        controller.service({SpeakerOutput::stereo, -30}, 1, send);
        assert(controller.status().fault && bus.attempts <= 15); bus.off();
        const auto attempts = bus.attempts;
        for (unsigned i = 0; i < 1000; ++i) controller.service({SpeakerOutput::left, 0}, 2 + i, send);
        assert(bus.attempts == attempts && controller.status().fault);
        controller.service({}, 2000, send);
        assert(!controller.status().fault); bus.off();
        controller.service({SpeakerOutput::left, -30}, 2001, send);
        assert(!controller.status().fault && controller.status().gain == 48);
    }
    // A permanently absent codec neither loops nor triggers a fatal handler.
    Codec dead; dead.clocks = true; dead.dead = true; Wm8960Speaker failed;
    const auto missing = [&](uint8_t r, uint16_t v) { return dead.write(r, v); };
    failed.service({SpeakerOutput::left, -30}, 1, missing);
    assert(failed.status().fault && dead.attempts == 8);
    for (unsigned i = 0; i < 1000; ++i) failed.service({SpeakerOutput::left, -30}, 1, missing);
    assert(dead.attempts == 8);
    failed.service({}, 2, missing); assert(dead.attempts == 12 && failed.status().fault);
    failed.service({}, 2, missing); assert(dead.attempts == 12);
    dead.dead = false; failed.service({}, 3, missing); assert(!failed.status().fault); dead.off();
    // NACK midway through a live gain change must also disable the amplifier.
    Codec hot; hot.clocks = true; Wm8960Speaker changing;
    const auto hot_write = [&](uint8_t r, uint16_t v) { return hot.write(r, v); };
    changing.service({SpeakerOutput::left, -30}, 1, hot_write);
    hot.fail_at = hot.attempts + 2;
    changing.service({SpeakerOutput::left, -30}, 1, hot_write);
    assert(changing.status().fault); hot.off();

    // Exercise the actual LCD/USB mirror renderer with the real font raster.
    const char* outputs[] = {"Off", "Left", "Right", "Stereo"};
    Parameter params[] = {{"Output", 0, 3, 1, 0, "", outputs}, {"Level", -60, 0, 1, -30, "dB"}};
    ModuleParameters controls{"Speaker", params};
    draw_speaker_panel(0, controls, {SpeakerOutput::off, 0, false, true}, true);
    image("build/speaker-off.ppm");
    const auto draws = labels.size();
    assert(!draw_speaker_panel(0, controls, {SpeakerOutput::off, 0, false, true}) && labels.size() == draws);
    params[0].set(1);
    draw_speaker_panel(0, controls, {SpeakerOutput::left, 71, false, true}, true);
    image("build/speaker-ramp.ppm");
    draw_speaker_panel(1, controls, {SpeakerOutput::left, 91, false, true}, true);
    image("build/speaker-level.ppm");
    assert(std::find(labels.begin(), labels.end(), "Active | actual -30 dB") != labels.end());
    draw_speaker_panel(0, controls, {SpeakerOutput::left, 91, true, true}, true);
    image("build/speaker-error.ppm");
    assert(std::find(labels.begin(), labels.end(), "I2C error: output not confirmed") != labels.end());
    std::puts("PASS: WM8960 hot enable/off/channel changes, 1dB ramp, no clock/HP/ADC changes, reserved/thermal bits, all enable-write failures, dead bus bound/latch/recovery, stable Save/Settings indices and real LCD rendering");
}
