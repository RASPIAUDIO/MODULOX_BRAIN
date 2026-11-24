#include "audio_output.h"
#include "runtime_diagnostics.h"

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

i2s_chan_handle_t start_i2s(unsigned sample_rate, unsigned block_frames, i2s_chan_handle_t* input)
{
    i2s_chan_handle_t tx = nullptr;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.dma_desc_num = 4;
    channel.dma_frame_num = block_frames;
    channel.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&channel, &tx, input));

    const i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_5,
            .ws = GPIO_NUM_16,
            .dout = GPIO_NUM_17,
            .din = input ? GPIO_NUM_4 : I2S_GPIO_UNUSED,
            .invert_flags = {},
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &config));
    if (input) ESP_ERROR_CHECK(i2s_channel_init_std_mode(*input, &config));
    ESP_ERROR_CHECK(i2s_channel_enable(tx));
    return tx;
}

namespace {

i2c_master_dev_handle_t headphone_codec;
int applied_headphone_gain = -1;
constexpr uint16_t initial_headphone_gain = 0x68;

esp_err_t try_write_register(i2c_master_dev_handle_t codec, uint8_t address, uint16_t value)
{
    const uint8_t message[] = {
        static_cast<uint8_t>((address << 1) | (value >> 8)),
        static_cast<uint8_t>(value),
    };
    esp_err_t result = ESP_OK;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        if (attempt != 0) vTaskDelay(pdMS_TO_TICKS(2));
        result = i2c_master_transmit(codec, message, sizeof(message), 50);
        if (result == ESP_OK) break;
    }
    return result;
}

void write_register(i2c_master_dev_handle_t codec, uint8_t address, uint16_t value)
{
    const esp_err_t result = try_write_register(codec, address, value);
    if (result != ESP_OK) {
        record_driver_error(DriverOperation::codec_write, result);
        ESP_ERROR_CHECK(result);
    }
}

}  // namespace

void set_headphone_volume(unsigned percent)
{
    if (!headphone_codec) {
        return;
    }
    // 1–100 covers -73 to 0 dB. At 77 the register is 0x68 (-17 dB).
    const uint16_t gain = percent ? 0x30 + (percent - 1) * 73 / 99 : 0;
    if (gain == applied_headphone_gain) return;

    esp_err_t result = try_write_register(headphone_codec, 0x02, gain);
    if (result == ESP_OK) result = try_write_register(headphone_codec, 0x03, 0x100 | gain);
    if (result == ESP_OK) {
        applied_headphone_gain = gain;
    } else {
        applied_headphone_gain = -1;
        ESP_LOGW("codec", "Cannot set headphone volume: %s", esp_err_to_name(result));
    }
}

void start_headphone_codec()
{
    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = GPIO_NUM_18;
    bus_config.scl_io_num = GPIO_NUM_11;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t bus = nullptr;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus));

    i2c_device_config_t device_config{};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = 0x1a;
    device_config.scl_speed_hz = 100000;
    i2c_master_dev_handle_t codec = nullptr;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &device_config, &codec));

    write_register(codec, 0x0f, 0x000);  // Reset.
    vTaskDelay(pdMS_TO_TICKS(10));
    write_register(codec, 0x05, 0x008);  // DAC soft mute during setup.
    write_register(codec, 0x19, 0x0c0);  // VREF and 50 kOhm VMID.
    write_register(codec, 0x07, 0x001);  // Left-justified, 16-bit, clock slave.

    // Preserve the existing Modulox clock configuration: PLL from the board's
    // codec master clock, N=7, K=0x86c226, SYSCLK divided by two for 44.1 kHz.
    write_register(codec, 0x34, 0x037);
    write_register(codec, 0x35, 0x086);
    write_register(codec, 0x36, 0x0c2);
    write_register(codec, 0x37, 0x026);
    write_register(codec, 0x1a, 0x001);  // Enable PLL.
    vTaskDelay(pdMS_TO_TICKS(10));
    write_register(codec, 0x04, 0x005);  // Select PLL and divide SYSCLK by two.
    write_register(codec, 0x08, 0x1c4);  // DCLK /16, BCLK /4.

    write_register(codec, 0x22, 0x100);  // Left DAC to left output mixer.
    write_register(codec, 0x25, 0x100);  // Right DAC to right output mixer.
    // Same INPUT1 -> PGA -> boost mixer -> ADC routing as the Arduino Granulizer.
    write_register(codec, 0x00, 0x117);  // Input PGA 0 dB, unmuted, update.
    write_register(codec, 0x01, 0x117);
    write_register(codec, 0x20, 0x138);  // INPUT1, MIC2B, +29 dB boost.
    write_register(codec, 0x21, 0x138);
    write_register(codec, 0x15, 0x1c3);  // ADC digital gain 0 dB, update.
    write_register(codec, 0x16, 0x1c3);
    write_register(codec, 0x19, 0x0fe);  // VREF, VMID, input mixers, ADCs, bias.
    write_register(codec, 0x2f, 0x03c);  // Input PGAs and output mixers.
    write_register(codec, 0x02, initial_headphone_gain);  // Headphone left at -17 dB.
    write_register(codec, 0x03, 0x100 | initial_headphone_gain);  // Headphone right, update both.
    write_register(codec, 0x1a, 0x1e3);  // DACs, headphones, OUT3 mixer, PLL.
    vTaskDelay(pdMS_TO_TICKS(100));
    write_register(codec, 0x05, 0x000);  // Release DAC mute.

    headphone_codec = codec;
    applied_headphone_gain = initial_headphone_gain;
}
