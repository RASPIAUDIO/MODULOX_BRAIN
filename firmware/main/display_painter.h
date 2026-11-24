#pragma once
#include "display_fonts.h"
#include "display_style.h"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace display {
inline uint16_t blend565(uint16_t ink, uint16_t background, unsigned alpha)
{
    const auto channel = [alpha](unsigned a, unsigned b) { return (a * alpha + b * (255 - alpha) + 127) / 255; };
    return (channel((ink >> 11) & 31, (background >> 11) & 31) << 11) |
        (channel((ink >> 5) & 63, (background >> 5) & 63) << 5) |
        channel(ink & 31, background & 31);
}

// Bounded scratch space; native-size alpha bitmaps stay in flash.
class TextRaster {
    struct Placed { const fonts::Glyph* glyph; int x; };
    std::array<Placed, 48> placed_{};
    std::array<uint16_t, 256> colors_{};
    const fonts::Font* font_ = nullptr;
    unsigned count_ = 0;
    int width_, height_, top_ = 0;
    uint16_t background_, highlight_, shadow_;
    TextStyle style_;
public:
    TextRaster(int w, int h, std::string_view label, int size, uint16_t ink, uint16_t background,
               TextStyle style = TextStyle::flat)
        : width_(w), height_(h), background_(background),
          highlight_(blend565(0xffff, background, 62)), shadow_(blend565(0, background, 150)), style_(style)
    {
        const fonts::Font* candidates[] = {&fonts::NotoSansBold11, &fonts::NotoSansBold15, &fonts::NotoSansMonoSCB20, &fonts::NotoSansBold36};
        const int available = std::max(0, w - (style == TextStyle::heading ? 96 : 8));
        label = label.substr(0, placed_.size());
        for (int candidate = std::clamp(size - 1, 1, 3); candidate >= 0; --candidate) {
            const auto* f = candidates[candidate];
            int advance = 0;
            for (unsigned char c : label) advance += f->glyphs[c >= 32 && c <= 126 ? c - 32 : '?' - 32].advance;
            if ((advance <= available && f->height <= h - 4) || candidate == 0) { font_ = f; break; }
        }
        int advance = 0;
        for (unsigned char c : label) {
            const auto& glyph = font_->glyphs[c >= 32 && c <= 126 ? c - 32 : '?' - 32];
            if (advance + glyph.advance > available) break;
            placed_[count_++] = {&glyph, advance};
            advance += glyph.advance;
        }
        const int left = (w - advance) / 2;
        for (unsigned i = 0; i < count_; ++i) placed_[i].x += left;
        top_ = (h - font_->height) / 2 + font_->ascent;
        for (unsigned alpha = 0; alpha < colors_.size(); ++alpha) colors_[alpha] = blend565(ink, background, alpha);
    }
    void row(int y, std::span<uint16_t> pixels) const
    {
        std::fill(pixels.begin(), pixels.end(), background_);
        if (style_ != TextStyle::flat) {
            if (y >= height_ - 2) std::fill(pixels.begin(), pixels.end(), uint16_t{0});
            else if (!y) std::fill(pixels.begin(), pixels.end(), highlight_);
            else if (y == height_ - 3) std::fill(pixels.begin(), pixels.end(), shadow_);
            else if (!pixels.empty()) { pixels.front() = highlight_; pixels.back() = shadow_; }
        }
        for (unsigned i = 0; i < count_; ++i) {
            const auto& p = placed_[i];
            const auto& g = *p.glyph;
            const int line = y - (top_ - g.top);
            if (line < 0 || line >= g.height) continue;
            const auto* alpha = font_->alpha + g.offset + line * g.width;
            for (unsigned col = 0; col < g.width; ++col) {
                const int x = p.x + g.x + col;
                if (x >= 0 && x < width_ && alpha[col]) pixels[x] = colors_[alpha[col]];
            }
        }
    }
};

inline void paint_response_row(std::span<uint16_t> pixels, std::span<const uint8_t> points, int row, int height)
{
    const int unity = 24 * (height - 1) / 84;
    for (unsigned x = 0; x < pixels.size(); ++x) {
        const int a = points[x ? x - 1 : 0], b = points[x];
        pixels[x] = row >= std::min(a, b) && row <= std::max(a, b) ? 0x47ed
            : !row || row == height - 1 || row == unity || x % 70 == 0 ? 0x2128
            : row > b ? 0x0144 : 0;
    }
}
}
