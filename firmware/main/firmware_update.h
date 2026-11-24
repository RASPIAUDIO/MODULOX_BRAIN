#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
#include "cJSON.h"

#include "board_profile_config.h"
inline constexpr unsigned firmware_slot_bytes = MODULOX_SLOT_BYTES;
inline constexpr const char* firmware_layout = MODULOX_BOARD_LAYOUT;
inline constexpr const char* firmware_model = MODULOX_BOARD_MODEL;
enum FirmwareReady : unsigned { firmware_storage_ready = 1, firmware_codec_ready = 2,
    firmware_controls_ready = 4, firmware_usb_ready = 8 };
void firmware_boot_begin();
void firmware_boot_ready(unsigned flags);
void firmware_boot_audio_ok();
bool firmware_update_active();
bool firmware_info(cJSON* object);
const char* firmware_update_begin(unsigned bytes, const char* sha256);
const char* firmware_update_write(unsigned offset, std::span<const char> bytes);
const char* firmware_update_finish();
void firmware_update_abort();
const char* firmware_request_rollback();
void firmware_update_service();
