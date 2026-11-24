#pragma once

#include "driver/i2s_std.h"

// Call on the audio core so the I2S interrupt is installed there.
i2s_chan_handle_t start_i2s(unsigned sample_rate, unsigned block_frames, i2s_chan_handle_t* input = nullptr);
void start_headphone_codec();
void set_headphone_volume(unsigned percent);
