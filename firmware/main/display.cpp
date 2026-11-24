#include "display.h"
#include "display_painter.h"
#include "runtime_diagnostics.h"

#include <algorithm>
#include <cstring>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace display {
namespace {

// Reuse the 5x7 font already bundled with the Arduino firmware.
#define PROGMEM
#include "../../libraries/TFT_eSPI/Fonts/glcdfont.c"
#undef PROGMEM

constexpr gpio_num_t backlight = GPIO_NUM_45;
constexpr int stripe_height = mirror_band_height;
static_assert(height % mirror_band_height == 0 && mirror_band_count < 255);
DMA_ATTR uint16_t pixels[width * stripe_height];
esp_lcd_panel_handle_t panel = nullptr;
SemaphoreHandle_t completion = nullptr;
SemaphoreHandle_t mirror_mutex = nullptr;
uint16_t* mirror = nullptr;
uint32_t revision = 1;
uint32_t band_revisions[mirror_band_count]{};

void mirror_stripe(int x, int y, int w, int lines)
{
    if (!mirror_available()) return;
    xSemaphoreTake(mirror_mutex, portMAX_DELAY);
    for (int line = 0; line < lines; ++line) {
        const int row = y + line;
        auto* destination = mirror + row * width + x;
        const auto* source = pixels + line * w;
        const size_t bytes = w * sizeof(*mirror);
        if (std::memcmp(destination, source, bytes) == 0) continue;
        std::memcpy(destination, source, bytes);
        if (++revision == 0) ++revision;
        band_revisions[row / mirror_band_height] = revision;
    }
    xSemaphoreGive(mirror_mutex);
}

bool transfer_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* context)
{
    BaseType_t awakened = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(context), &awakened);
    return awakened == pdTRUE;
}

void transfer_stripe(int x, int y, int w, int lines)
{
    const esp_err_t result = esp_lcd_panel_draw_bitmap(panel, x, y, x + w, y + lines, pixels);
    if (result != ESP_OK) {
        record_driver_error(DriverOperation::display_draw, result);
        ESP_ERROR_CHECK(result);
    }
    if (xSemaphoreTake(completion, pdMS_TO_TICKS(1000)) != pdTRUE) {
        record_driver_error(DriverOperation::display_wait, ESP_ERR_TIMEOUT);
        ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
    }
    mirror_stripe(x, y, w, lines);
}

}  // namespace

void init_mirror()
{
    mirror = static_cast<uint16_t*>(heap_caps_calloc(width * height, sizeof(*mirror), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!mirror) return;
    mirror_mutex = xSemaphoreCreateMutex();
    if (!mirror_mutex) {
        heap_caps_free(mirror);
        mirror = nullptr;
    }
}

bool mirror_available()
{
    return mirror != nullptr;
}

unsigned read_mirror(uint32_t since, unsigned first_band, std::span<char> output)
{
    constexpr unsigned header_bytes = 6;
    constexpr unsigned band_bytes = width * mirror_band_height * sizeof(*mirror);
    if (!mirror_available() || first_band >= mirror_band_count || output.size() < header_bytes + 1 + band_bytes) return 0;

    // Header: revision (little endian), continuation band (255 at the end), band count.
    xSemaphoreTake(mirror_mutex, portMAX_DELAY);
    for (unsigned i = 0; i < 4; ++i) output[i] = static_cast<char>(revision >> (8 * i));
    unsigned band = first_band;
    unsigned count = 0;
    unsigned offset = header_bytes;
    for (; band < mirror_band_count; ++band) {
        const uint32_t age = band_revisions[band] - since;
        if (since && (!age || age > UINT32_MAX / 2)) continue;
        if (output.size() - offset < 1 + band_bytes) break;
        output[offset++] = static_cast<char>(band);
        std::memcpy(output.data() + offset, mirror + band * width * mirror_band_height, band_bytes);
        offset += band_bytes;
        ++count;
    }
    output[4] = static_cast<char>(band == mirror_band_count ? 255 : band);
    output[5] = static_cast<char>(count);
    xSemaphoreGive(mirror_mutex);
    return offset;
}

void init()
{
    ESP_ERROR_CHECK(gpio_set_direction(backlight, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(backlight, 1));  // Backlight is active low.

    spi_bus_config_t bus{};
    bus.mosi_io_num = GPIO_NUM_15;
    bus.miso_io_num = -1;
    bus.sclk_io_num = GPIO_NUM_14;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = sizeof(pixels);
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    completion = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(completion ? ESP_OK : ESP_ERR_NO_MEM);
    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.cs_gpio_num = GPIO_NUM_40;
    io_config.dc_gpio_num = GPIO_NUM_8;
    io_config.pclk_hz = 40000000;
    io_config.trans_queue_depth = 1;
    io_config.on_color_trans_done = transfer_done;
    io_config.user_ctx = completion;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    esp_lcd_panel_io_handle_t io = nullptr;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io));

    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = GPIO_NUM_9;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    // Match TFT_eSPI rotation 3 and its 20-pixel offset for the 240 x 280 panel.
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 20, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));

    text(0, 0, width, height, {}, 1, 0, 0);
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    ESP_ERROR_CHECK(gpio_set_level(backlight, 0));
}

void text(int x, int y, int w, int h, std::string_view label, int scale,
          uint16_t foreground, uint16_t background, TextStyle style)
{
    if (scale > 1 || style != TextStyle::flat) {
        const TextRaster raster(w, h, label, scale, foreground, background, style);
        for (int top = 0; top < h; top += stripe_height) {
            const int lines = std::min(stripe_height, h - top);
            for (int line = 0; line < lines; ++line) {
                const std::span row{pixels + line * w, unsigned(w)};
                raster.row(top + line, row);
                for (auto& pixel : row) pixel = __builtin_bswap16(pixel);
            }
            transfer_stripe(x, y + top, w, lines);
        }
        return;
    }
    constexpr int glyph_width = 5;
    constexpr int glyph_height = 7;
    constexpr int pitch = glyph_width + 1;
    label = label.substr(0, w / pitch);
    if (!label.empty()) {
        scale = std::min(scale, w / (static_cast<int>(label.size()) * pitch));
    }
    const int text_x = (w - static_cast<int>(label.size()) * pitch * scale) / 2;
    const int text_y = (h - glyph_height * scale) / 2;
    foreground = __builtin_bswap16(foreground);
    background = __builtin_bswap16(background);

    for (int top = 0; top < h; top += stripe_height) {
        const int lines = std::min(stripe_height, h - top);
        std::fill_n(pixels, w * lines, background);
        for (int line = 0; line < lines; ++line) {
            const int dy = top + line - text_y;
            if (dy < 0 || dy >= glyph_height * scale) {
                continue;
            }
            const unsigned bit = 1u << (dy / scale);
            for (std::size_t i = 0; i < label.size(); ++i) {
                const auto* glyph = font + glyph_width * static_cast<unsigned char>(label[i]);
                auto* dest = pixels + line * w + text_x + i * pitch * scale;
                for (int col = 0; col < glyph_width; ++col) {
                    if ((glyph[col] & bit) != 0) {
                        std::fill_n(dest + col * scale, scale, foreground);
                    }
                }
            }
        }
        transfer_stripe(x, y + top, w, lines);
    }
}

void heading(std::string_view label)
{
    text(0, 0, width, 50, label, 3, 0xffff, 0x39c7, TextStyle::heading);
}

void response_curve(int y, int h, std::span<const uint8_t, width> points)
{
    for (int top = 0; top < h; top += stripe_height) {
        const int lines = std::min(stripe_height, h - top);
        for (int line = 0; line < lines; ++line) {
            const std::span row{pixels + line * width, unsigned(width)};
            paint_response_row(row, points, top + line, h);
            for (auto& pixel : row) pixel = __builtin_bswap16(pixel);
        }
        transfer_stripe(0, y + top, width, lines);
    }
}

void waveform(int y, int h, std::span<const uint8_t, width> points, int start, int end)
{
    const int zero = (h - 1) / 2;
    const uint16_t grid = __builtin_bswap16(0x2128);
    const uint16_t fill = __builtin_bswap16(0x0328);
    const uint16_t trace = __builtin_bswap16(0x47ed);
    const uint16_t start_color = __builtin_bswap16(0x07e0);
    const uint16_t end_color = __builtin_bswap16(0xfd20);
    for (int top = 0; top < h; top += stripe_height) {
        const int lines = std::min(stripe_height, h - top);
        std::fill_n(pixels, width * lines, 0);
        for (int x = 0; x < width; ++x) {
            const int current = points[x];
            const int previous = points[std::max(0, x - 1)];
            for (int line = 0; line < lines; ++line) {
                const int row = top + line;
                uint16_t& pixel = pixels[line * width + x];
                if (row >= std::min(previous, current) && row <= std::max(previous, current)) {
                    pixel = trace;
                } else if (row == zero || x % 70 == 0 || row == 0 || row == h - 1) {
                    pixel = grid;
                } else if (row >= std::min(current, zero) && row <= std::max(current, zero)) {
                    pixel = fill;
                }
                if (start >= 0 && x >= start && x < start + 2) pixel = start_color;
                if (end >= 0 && x <= end && x > end - 2) pixel = end_color;
            }
        }
        transfer_stripe(0, y + top, width, lines);
    }
}

}
