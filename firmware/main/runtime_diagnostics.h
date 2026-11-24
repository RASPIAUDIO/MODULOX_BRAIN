#pragma once

#include "esp_err.h"

enum class PatchLoadStage : unsigned { idle, read, waveform, effects, pause, apply, resume };
enum class DriverOperation : unsigned { none, display_draw, display_wait, codec_write, audio_write };

void start_runtime_diagnostics();
void record_patch_load(unsigned index, PatchLoadStage stage);
void record_driver_error(DriverOperation operation, esp_err_t error);
int format_runtime_diagnostics(char* buffer, unsigned capacity);
