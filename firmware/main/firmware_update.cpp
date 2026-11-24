#include "firmware_update.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "mbedtls/sha256.h"

namespace {
constexpr const char* tag = "firmware";
std::atomic<bool> active{false}, pending{false};
std::atomic<unsigned> readiness{0}, audio_blocks{0};
std::atomic<uint32_t> last_audio_ms{0};
esp_timer_handle_t boot_timer;
int64_t boot_started;
esp_ota_handle_t handle = 0;
const esp_partition_t* target = nullptr;
mbedtls_sha256_context digest;
bool digest_ready = false, first_chunk = true;
unsigned expected_size = 0, received_size = 0;
uint8_t expected_hash[32]{};
int64_t last_byte = 0, reboot_at = 0;
bool rollback_requested = false;
uint32_t flash_bytes = 0;
size_t psram_bytes = 0;
bool memory_ok = false;

bool layout_ok() {
    const auto* a = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    const auto* b = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    const auto* data = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr);
    return memory_ok && a && b && data && a->address == MODULOX_OTA0 && b->address == MODULOX_OTA1 &&
        a->size == firmware_slot_bytes && b->size == firmware_slot_bytes && data->address == 0x1f0000 && data->size == 0x2000;
}
void boot_check(void*) {
    if (!pending.load()) return;
    const int64_t elapsed = esp_timer_get_time() - boot_started;
    const unsigned required = firmware_storage_ready | firmware_codec_ready | firmware_controls_ready | firmware_usb_ready;
    const uint32_t now = esp_timer_get_time() / 1000;
#ifdef MODULOX_ROLLBACK_TEST
    constexpr bool injected_failure = true;
    constexpr int64_t deadline = 7000000;
#else
    constexpr bool injected_failure = false;
    constexpr int64_t deadline = 30000000;
#endif
    if (!injected_failure && memory_ok && elapsed >= 5000000 && (readiness.load() & required) == required && audio_blocks.load() >= 100 && now - last_audio_ms.load() < 500) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            pending.store(false); esp_timer_stop(boot_timer);
            ESP_LOGI(tag, "Boot self-test passed; firmware confirmed"); return;
        }
    }
    if (elapsed >= deadline) {
        ESP_LOGE(tag, "Boot not confirmed; rebooting for rollback");
        // The bootloader rejects an unconfirmed PENDING_VERIFY image on reboot.
        esp_restart();
    }
}
int nibble(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
const char* fail(const char* reason) { firmware_update_abort(); return reason; }
}

void firmware_boot_begin() {
    psram_bytes = esp_psram_is_initialized() ? esp_psram_get_size() : 0;
    memory_ok = esp_flash_get_size(nullptr, &flash_bytes) == ESP_OK &&
        flash_bytes == MODULOX_FLASH_BYTES && psram_bytes == MODULOX_PSRAM_BYTES;
    ESP_LOGI(tag, "Board %s: flash %lu, PSRAM %u, memory %s", MODULOX_BOARD_ID,
        (unsigned long)flash_bytes, (unsigned)psram_bytes, memory_ok ? "OK" : "MISMATCH");
    const auto* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY) return;
    pending.store(true); boot_started = esp_timer_get_time();
    esp_timer_create_args_t args{}; args.callback = boot_check; args.name = "boot_verify";
    ESP_ERROR_CHECK(esp_timer_create(&args, &boot_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(boot_timer, 250000));
}
void firmware_boot_ready(unsigned flags) { readiness.fetch_or(flags); }
void firmware_boot_audio_ok() {
    if (pending.load(std::memory_order_relaxed)) {
        audio_blocks.fetch_add(1, std::memory_order_relaxed);
        last_audio_ms.store(uint32_t(esp_timer_get_time() / 1000), std::memory_order_relaxed);
    }
}
bool firmware_update_active() { return active.load(std::memory_order_acquire); }
bool firmware_info(cJSON* object) {
    const auto* app = esp_app_get_description(); const auto* running = esp_ota_get_running_partition();
    char build[65]{};
    constexpr char hex[] = "0123456789abcdef";
    // esp_app_get_elf_sha256 may return only CONFIG_APP_RETRIEVE_LEN_ELF_SHA
    // characters. The update protocol always exposes the full descriptor hash.
    for (unsigned i = 0; i < 32; ++i) { build[2*i] = hex[app->app_elf_sha256[i] >> 4]; build[2*i+1] = hex[app->app_elf_sha256[i] & 15]; }
    return cJSON_AddStringToObject(object, "model", firmware_model) &&
        cJSON_AddStringToObject(object, "hardwareProfile", MODULOX_BOARD_ID) &&
        cJSON_AddNumberToObject(object, "flashBytes", flash_bytes) &&
        cJSON_AddNumberToObject(object, "psramBytes", psram_bytes) &&
        cJSON_AddNumberToObject(object, "sampleStorageBytes", MODULOX_SAMPLE_BYTES) &&
        cJSON_AddBoolToObject(object, "memoryOk", memory_ok) &&
        cJSON_AddStringToObject(object, "chip", "esp32s3") &&
        cJSON_AddStringToObject(object, "version", app->version) &&
        cJSON_AddStringToObject(object, "project", app->project_name) &&
        cJSON_AddStringToObject(object, "buildId", build) &&
        cJSON_AddStringToObject(object, "layout", firmware_layout) &&
        cJSON_AddStringToObject(object, "running", running ? running->label : "unknown") &&
        cJSON_AddNumberToObject(object, "protocol", 1) &&
        cJSON_AddNumberToObject(object, "slotBytes", firmware_slot_bytes) &&
        cJSON_AddBoolToObject(object, "usbUpdate", layout_ok()) &&
        cJSON_AddBoolToObject(object, "pendingVerify", pending.load()) &&
        cJSON_AddBoolToObject(object, "busy", active.load()) &&
        cJSON_AddBoolToObject(object, "canRollback", esp_ota_check_rollback_is_possible());
}
const char* firmware_update_begin(unsigned bytes, const char* sha) {
    if (active.load() || pending.load()) return "firmware-busy";
    if (!layout_ok()) return "firmware-layout";
    if (!sha || std::strlen(sha) != 64 || bytes < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) || bytes > firmware_slot_bytes) return "firmware-invalid";
    for (unsigned i = 0; i < 32; ++i) {
        const int a = nibble(sha[2*i]), b = nibble(sha[2*i+1]);
        if (a < 0 || b < 0) return "firmware-invalid";
        expected_hash[i] = (a << 4) | b;
    }
    target = esp_ota_get_next_update_partition(nullptr);
    if (!target || target == esp_ota_get_running_partition() || target->size != firmware_slot_bytes) return "firmware-layout";
    active.store(true); expected_size = bytes; received_size = 0; first_chunk = true; last_byte = esp_timer_get_time();
    if (esp_ota_begin(target, bytes, &handle) != ESP_OK) return fail("firmware-storage");
    mbedtls_sha256_init(&digest); digest_ready = true;
    if (mbedtls_sha256_starts(&digest, 0) != 0) return fail("firmware-storage");
    return nullptr;
}
const char* firmware_update_write(unsigned offset, std::span<const char> bytes) {
    if (!handle || offset != received_size || bytes.empty() || bytes.size() > 4096 || bytes.size() > expected_size - received_size) return fail("firmware-invalid");
    if (first_chunk) {
        constexpr auto descriptor_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
        if (bytes.size() < descriptor_offset + sizeof(esp_app_desc_t)) return fail("firmware-invalid");
        esp_image_header_t image; esp_app_desc_t app;
        std::memcpy(&image, bytes.data(), sizeof(image)); std::memcpy(&app, bytes.data() + descriptor_offset, sizeof(app));
        if (image.magic != ESP_IMAGE_HEADER_MAGIC || image.chip_id != ESP_CHIP_ID_ESP32S3 || app.magic_word != ESP_APP_DESC_MAGIC_WORD ||
            std::strncmp(app.project_name, esp_app_get_description()->project_name, sizeof(app.project_name)) != 0) return fail("firmware-incompatible");
        first_chunk = false;
    }
    if (esp_ota_write(handle, bytes.data(), bytes.size()) != ESP_OK || mbedtls_sha256_update(&digest, reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) != 0) return fail("firmware-storage");
    received_size += bytes.size(); last_byte = esp_timer_get_time(); return nullptr;
}
const char* firmware_update_finish() {
    if (!handle || received_size != expected_size) return fail("firmware-incomplete");
    uint8_t actual[32];
    if (mbedtls_sha256_finish(&digest, actual) != 0 || std::memcmp(actual, expected_hash, sizeof(actual))) return fail("firmware-hash");
    mbedtls_sha256_free(&digest); digest_ready = false;
    const auto result = esp_ota_end(handle); handle = 0;
    if (result != ESP_OK) return fail("firmware-invalid");
    if (esp_ota_set_boot_partition(target) != ESP_OK) return fail("firmware-storage");
    reboot_at = esp_timer_get_time() + 1000000; return nullptr;
}
void firmware_update_abort() {
    if (reboot_at) return; // A committed image must reboot even when USB disconnects.
    if (handle) esp_ota_abort(handle);
    handle = 0;
    if (digest_ready) mbedtls_sha256_free(&digest);
    digest_ready = false; expected_size = received_size = 0; target = nullptr; active.store(false);
}
const char* firmware_request_rollback() {
    if (active.load() || pending.load()) return "firmware-busy";
    if (!esp_ota_check_rollback_is_possible()) return "firmware-no-rollback";
    rollback_requested = true; active.store(true); reboot_at = esp_timer_get_time() + 1000000; return nullptr;
}
void firmware_update_service() {
    const auto now = esp_timer_get_time();
    if (reboot_at && now >= reboot_at) {
        if (rollback_requested) {
            const auto result = esp_ota_mark_app_invalid_rollback_and_reboot();
            ESP_LOGE(tag, "Rollback failed: %s", esp_err_to_name(result));
            reboot_at = 0; rollback_requested = false; active.store(false);
        } else esp_restart();
    } else if (handle && now - last_byte > 15000000) firmware_update_abort();
}
