#pragma once

#include "driver/i2s_std.h"
#include "parameter.h"
#include "wm8960_speaker.h"

// Call on the audio core so the I2S interrupt is installed there.
i2s_chan_handle_t start_i2s(unsigned sample_rate, unsigned block_frames, i2s_chan_handle_t* input = nullptr);
void start_headphone_codec();
void set_headphone_volume(unsigned percent);

// Global, volatile hardware controls: independent of patches and MIDI Learn.
ModuleParameters speaker_controls();
SpeakerStatus speaker_status();
void stop_speaker(); // Also permits an explicit retry of Off after an I2C fault.
