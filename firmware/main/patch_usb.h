#pragma once

#include "patch_store.h"
#include "recording.h"

void service_patch_usb(PatchStore& patches, PatchStore::Modules modules, WaveStore& waves,
                       SampleStore& samples, bool (*prune_waves)(), bool recording = false,
                       SampleDeleteStatus (*delete_sample)(const char*) = nullptr);
bool patch_usb_transfer_active();
