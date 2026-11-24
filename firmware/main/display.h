#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include "display_style.h"

namespace display {

inline constexpr int width = 280;
inline constexpr int height = 240;
inline constexpr int mirror_band_height = 8;
inline constexpr int mirror_band_count = height / mirror_band_height;

void init_mirror();
bool mirror_available();
// RGB565 in LCD byte order. A zero revision requests every band.
unsigned read_mirror(uint32_t since, unsigned first_band, std::span<char> output);
void init();
void text(int x, int y, int w, int h, std::string_view label, int scale,
          uint16_t foreground, uint16_t background, TextStyle style = TextStyle::flat);
void heading(std::string_view label);
inline void button(int x, int y, int w, int h, std::string_view label, int size, uint16_t ink, uint16_t background)
{ text(x, y, w, h, label, size, ink, background, TextStyle::button); }
void waveform(int y, int h, std::span<const uint8_t, width> points, int start = -1, int end = -1);
void response_curve(int y, int h, std::span<const uint8_t, width> points);

}
