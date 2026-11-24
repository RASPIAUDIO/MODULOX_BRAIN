#include "waves.h"

#include <algorithm>
#include <cstring>

#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "nvs_flash.h"

namespace {
constexpr char partition[] = "waves";
constexpr char keyspace[] = "cycles";
}

bool valid_wave_key(std::string_view key)
{
    return key.size() == wave_key_size && key.find_first_not_of("0123456789abcdef") ==
           std::string_view::npos;
}

void WaveStore::mount()
{
    esp_err_t result = nvs_flash_init_partition(partition);
    if (result == ESP_OK) {
        result = nvs_open_from_partition(partition, keyspace, NVS_READWRITE, &handle_);
    }
    if (result != ESP_OK) {
        ESP_LOGW("waves", "Cannot mount wave storage: %s", esp_err_to_name(result));
    }
}

bool WaveStore::contains(const char* key) const
{
    size_t size = 0;
    return handle_ && nvs_get_blob(handle_, key, nullptr, &size) == ESP_OK && size == sizeof(WaveData);
}

bool WaveStore::read(const char* key, WaveData& wave) const
{
    size_t size = sizeof(wave);
    return handle_ && nvs_get_blob(handle_, key, &wave, &size) == ESP_OK && size == sizeof(wave);
}

WaveStore::Result WaveStore::write(const char* key, std::span<const char> bytes)
{
    if (!valid_wave_key(key) || bytes.size() != sizeof(WaveData) ||
        !std::memchr(bytes.data(), '\0', sizeof(WaveData::name))) {
        return invalid;
    }
    constexpr auto name_size = sizeof(WaveData::name);
    const auto* pcm = reinterpret_cast<const unsigned char*>(bytes.data() + name_size);
    unsigned char digest[32];
    if (mbedtls_sha256(pcm, bytes.size() - name_size, digest, 0) != 0) {
        return storage;
    }
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < wave_key_size / 2; ++i) {
        if (key[2 * i] != hex[digest[i] >> 4] || key[2 * i + 1] != hex[digest[i] & 15]) {
            return invalid;
        }
    }
    if (!handle_) {
        return storage;
    }
    if (contains(key)) {
        return ok;
    }
    const auto result = nvs_set_blob(handle_, key, bytes.data(), bytes.size());
    if (result == ESP_ERR_NVS_NOT_ENOUGH_SPACE) {
        return full;
    }
    return result == ESP_OK && nvs_commit(handle_) == ESP_OK ? ok : storage;
}

bool WaveStore::prune(std::span<const WaveKey> referenced)
{
    if (!handle_) {
        return false;
    }
    nvs_iterator_t iterator = nullptr;
    auto status = nvs_entry_find_in_handle(handle_, NVS_TYPE_BLOB, &iterator);
    while (status == ESP_OK) {
        nvs_entry_info_t info{};
        nvs_entry_info(iterator, &info);
        if (std::none_of(referenced.begin(), referenced.end(), [&](const WaveKey& key) {
                return std::strcmp(key.data(), info.key) == 0;
            })) {
            const auto erased = nvs_erase_key(handle_, info.key);
            if (erased != ESP_OK) {
                nvs_release_iterator(iterator);
                return false;
            }
        }
        status = nvs_entry_next(&iterator);
    }
    nvs_release_iterator(iterator);
    return status == ESP_ERR_NVS_NOT_FOUND && nvs_commit(handle_) == ESP_OK;
}
