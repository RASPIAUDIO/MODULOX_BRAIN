#pragma once
#include "esp_err.h"
extern "C" esp_err_t dsps_biquad_f32_ansi(const float*, float*, int, float*, float*);
// Run Espressif's portable implementation instead of the Xtensa assembly.
#define dsps_biquad_f32_aes3 dsps_biquad_f32_ansi
