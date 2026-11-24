#include "sample_store.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char root[] = "/samples";
constexpr char partition[] = "samples";
constexpr char temporary[] = "/samples/.upload";
using FilePath = char[sizeof(root) + wave_key_size + 1];

void path_for(FilePath& path, const char* key)
{
    std::snprintf(path, sizeof(path), "%s/%s", root, key);
}

const char* check_partition_contents()
{
    const auto* area = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, partition);
    if (!area) return "partition-missing";
    const std::unique_ptr<uint32_t[]> block(new (std::nothrow) uint32_t[sample_chunk_bytes / sizeof(uint32_t)]);
    if (!block) return "storage-error";
    for (size_t offset = 0; offset < area->size; offset += sample_chunk_bytes) {
        if (esp_partition_read(area, offset, block.get(), sample_chunk_bytes) != ESP_OK) return "storage-error";
        for (unsigned i = 0; i < sample_chunk_bytes / sizeof(uint32_t); ++i) {
            if (block[i] != UINT32_MAX) return "storage-not-empty";
        }
        if (offset % (64 * 1024) == 0) vTaskDelay(1);
    }
    return nullptr;
}
}

void SampleStore::mount()
{
    esp_vfs_spiffs_conf_t config{};
    config.base_path = root;
    config.partition_label = partition;
    config.max_files = 3;
    auto result = esp_vfs_spiffs_register(&config);
    if (result != ESP_OK) {
        storage_status_ = check_partition_contents();
        if (!storage_status_) {
            config.format_if_mount_failed = true;
            result = esp_vfs_spiffs_register(&config);
            storage_status_ = "storage-error";
        }
    }
    mounted_ = result == ESP_OK;
    if (mounted_) std::remove(temporary);
    else ESP_LOGW("samples", "Sample storage unavailable: %s", esp_err_to_name(result));
}

bool SampleStore::available() const
{
    return mounted_ && heap_caps_get_total_size(MALLOC_CAP_SPIRAM) != 0;
}

const char* SampleStore::status() const
{
    if (!mounted_) return storage_status_;
    return available() ? "ready" : "psram-unavailable";
}

unsigned SampleStore::size(const char* key) const
{
    const std::lock_guard guard(mutex_);
    if (!mounted_ || !valid_wave_key(key)) return 0;
    FilePath path;
    path_for(path, key);
    struct stat info{};
    return stat(path, &info) == 0 && info.st_size >= 4 &&
        info.st_size <= max_sample_bytes && info.st_size % 2 == 0 ? info.st_size : 0;
}

int SampleStore::read(const char* key, unsigned offset, std::span<char> bytes) const
{
    const std::lock_guard guard(mutex_);
    const unsigned length = size(key);
    if (!length || offset > length) return -1;
    FilePath path;
    path_for(path, key);
    const std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path, "rb"), std::fclose);
    if (!file || std::fseek(file.get(), offset, SEEK_SET) != 0) return -1;
    const unsigned count = std::min<unsigned>(bytes.size(), length - offset);
    return std::fread(bytes.data(), 1, count, file.get()) == count ? static_cast<int>(count) : -1;
}

bool SampleStore::load(const char* key, SampleData& sample) const
{
    const std::lock_guard guard(mutex_);
    const unsigned bytes = size(key);
    if (!bytes) return false;
    SampleData next;
    next.pcm.reset(static_cast<int16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!next.pcm) return false;
    FilePath path;
    path_for(path, key);
    const std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path, "rb"), std::fclose);
    const std::unique_ptr<char[]> scratch(new (std::nothrow) char[sample_chunk_bytes]);
    if (!file || !scratch) return false;
    auto* out = reinterpret_cast<char*>(next.pcm.get());
    for (unsigned offset = 0; offset < bytes; offset += sample_chunk_bytes) {
        const unsigned count = std::min(sample_chunk_bytes, bytes - offset);
        if (std::fread(scratch.get(), 1, count, file.get()) != count) return false;
        std::memcpy(out + offset, scratch.get(), count);
    }
    next.frames = bytes / sizeof(int16_t);
    sample = std::move(next);
    return true;
}

bool SampleStore::preview(const char* key, std::span<SamplePeak, sample_preview_points> peaks, unsigned& frames) const
{
    const std::lock_guard guard(mutex_);
    const unsigned bytes = size(key);
    if (!bytes) return false;
    FilePath path; path_for(path, key);
    const std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path, "rb"), std::fclose);
    const std::unique_ptr<int16_t[]> scratch(new (std::nothrow) int16_t[sample_chunk_bytes / 2]);
    if (!file || !scratch) return false;
    frames = bytes / 2;
    std::fill(peaks.begin(), peaks.end(), SamplePeak{});
    if (frames < sample_preview_points) {
        if (std::fread(scratch.get(), 2, frames, file.get()) != frames) return false;
        for (unsigned bin = 0; bin < peaks.size(); ++bin) {
            const int16_t value = scratch[bin * frames / sample_preview_points];
            peaks[bin] = {std::min<int16_t>(0, value), std::max<int16_t>(0, value)};
        }
        return true;
    }
    unsigned bin = 0, next = frames / sample_preview_points;
    for (unsigned offset = 0; offset < frames;) {
        const unsigned count = std::min<unsigned>(sample_chunk_bytes / 2, frames - offset);
        if (std::fread(scratch.get(), 2, count, file.get()) != count) return false;
        for (unsigned i = 0; i < count; ++i) {
            if (offset + i >= next && bin + 1 < peaks.size()) {
                ++bin; next = (bin + 1) * frames / sample_preview_points;
            }
            peaks[bin].low = std::min(peaks[bin].low, scratch[i]);
            peaks[bin].high = std::max(peaks[bin].high, scratch[i]);
        }
        offset += count;
    }
    return true;
}

bool SampleStore::writing() const
{
    const std::lock_guard guard(mutex_);
    return upload_ || recording_reserved_;
}

bool SampleStore::remove(const char* key)
{
    const std::lock_guard guard(mutex_);
    if (writing() || !size(key)) return false;
    FilePath path; path_for(path, key);
    if (std::remove(path) != 0) return false;
    char label[32];
    for (const char* suffix : {"name", "old"}) {
        std::snprintf(label, sizeof(label), "%s/%s.%s", root, key, suffix);
        std::remove(label);
    }
    revision_.fetch_add(1, std::memory_order_release);
    return true;
}

std::vector<WaveKey> SampleStore::keys() const
{
    const std::lock_guard guard(mutex_);
    std::vector<WaveKey> keys;
    DIR* directory = mounted_ ? opendir(root) : nullptr;
    if (!directory) return keys;
    while (const auto* entry = readdir(directory)) {
        if (valid_wave_key(entry->d_name)) {
            WaveKey key{};
            std::memcpy(key.data(), entry->d_name, wave_key_size);
            keys.push_back(key);
        }
    }
    closedir(directory);
    std::sort(keys.begin(), keys.end());
    return keys;
}

bool SampleStore::read_name(const char* key, std::span<char, 32> name) const
{
    const std::lock_guard guard(mutex_);
    if (!mounted_ || !valid_wave_key(key)) return false;
    char path[32];
    std::snprintf(path, sizeof(path), "%s/%s.name", root, key);
    std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path, "rb"), std::fclose);
    if (!file) {
        std::snprintf(path, sizeof(path), "%s/%s.old", root, key);
        file.reset(std::fopen(path, "rb"));
    }
    if (!file) return false;
    const auto count = std::fread(name.data(), 1, name.size() - 1, file.get());
    name[count] = '\0';
    return count != 0;
}

bool SampleStore::write_name(const char* key, std::span<const char> name)
{
    const std::lock_guard guard(mutex_);
    if (recording_reserved_) return false;
    if (!size(key) || name.empty() || name.size() > 31 ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c > 126; })) return false;
    char path[32];
    std::snprintf(path, sizeof(path), "%s/%s.name", root, key);
    char existing[32];
    if (read_name(key, existing) && std::string_view(existing) == std::string_view(name.data(), name.size())) return true;
    constexpr char pending_name[] = "/samples/.name";
    FILE* file = std::fopen(pending_name, "wb");
    if (!file) return false;
    const bool written = std::fwrite(name.data(), 1, name.size(), file) == name.size();
    const bool valid = std::fclose(file) == 0 && written;
    // SPIFFS rename does not replace an existing destination. Keep the old label
    // recoverable until the new label has been installed, including across reset.
    char backup[32];
    std::snprintf(backup, sizeof(backup), "%s/%s.old", root, key);
    struct stat info{};
    if (valid && stat(path, &info) == 0) {
        std::remove(backup);
        if (std::rename(path, backup) != 0) { std::remove(pending_name); return false; }
    }
    if (valid && std::rename(pending_name, path) == 0) {
        std::remove(backup);
        revision_.fetch_add(1, std::memory_order_release);
        return true;
    }
    std::rename(backup, path);
    std::remove(pending_name);
    return false;
}

bool SampleStore::begin(const char* key, unsigned bytes)
{
    const std::lock_guard guard(mutex_);
    if (recording_reserved_) return false;
    abort();
    if (!available() || bytes < 4 || bytes > max_sample_bytes || bytes % 2 || !valid_wave_key(key)) return false;
    upload_ = std::fopen(temporary, "wb");
    if (!upload_) return false;
    std::memcpy(key_.data(), key, key_.size());
    remaining_ = bytes;
    mbedtls_sha256_init(&digest_);
    if (mbedtls_sha256_starts(&digest_, 0) == 0) return true;
    abort();
    return false;
}

bool SampleStore::append(std::span<const char> bytes)
{
    const std::lock_guard guard(mutex_);
    if (!upload_ || bytes.size() > remaining_) return false;
    if (std::fwrite(bytes.data(), 1, bytes.size(), upload_) != bytes.size() ||
        mbedtls_sha256_update(&digest_, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) != 0) {
        abort();
        return false;
    }
    remaining_ -= bytes.size();
    return true;
}

bool SampleStore::commit()
{
    const std::lock_guard guard(mutex_);
    if (!upload_ || remaining_) return false;
    unsigned char hash[32];
    bool valid = mbedtls_sha256_finish(&digest_, hash) == 0;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; valid && i < wave_key_size / 2; ++i) {
        valid = key_[i * 2] == hex[hash[i] >> 4] && key_[i * 2 + 1] == hex[hash[i] & 15];
    }
    mbedtls_sha256_free(&digest_);
    valid = std::fclose(upload_) == 0 && valid;
    upload_ = nullptr;
    FilePath path;
    path_for(path, key_.data());
    if (valid && size(key_.data())) {
        std::remove(temporary);
        return true;
    }
    if (valid && std::rename(temporary, path) == 0) {
        revision_.fetch_add(1, std::memory_order_release);
        return true;
    }
    std::remove(temporary);
    return false;
}

void SampleStore::abort()
{
    const std::lock_guard guard(mutex_);
    if (!upload_) return;
    std::fclose(upload_);
    upload_ = nullptr;
    remaining_ = 0;
    mbedtls_sha256_free(&digest_);
    std::remove(temporary);
}

bool SampleStore::save_recording(std::span<const int16_t> pcm, WaveKey& key, std::span<char, 32> name)
{
    const std::lock_guard guard(mutex_);
    struct Restore { bool& value; bool previous; ~Restore() { value = previous; } } restore{recording_reserved_, recording_reserved_};
    recording_reserved_ = false;
    // Do not take over an unfinished USB upload. The recording stays in PSRAM for retry.
    if (upload_ || !available() || pcm.size() < 2 || pcm.size_bytes() > max_sample_bytes) return false;
    unsigned char hash[32];
    if (mbedtls_sha256(reinterpret_cast<const unsigned char*>(pcm.data()), pcm.size_bytes(), hash, 0)) return false;
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < wave_key_size / 2; ++i) {
        key[2 * i] = hex[hash[i] >> 4]; key[2 * i + 1] = hex[hash[i] & 15];
    }
    key[wave_key_size] = '\0';
    if (size(key.data()) && read_name(key.data(), name)) return true;
    unsigned number = 0;
    for (const auto& existing : keys()) {
        char label[32]{}; unsigned at = 0; char tail;
        if (read_name(existing.data(), label) && std::sscanf(label, "enregistrement%u%c", &at, &tail) == 1)
            number = std::max(number, at);
    }
    std::snprintf(name.data(), name.size(), "enregistrement%u", number + 1);
    if (!size(key.data())) {
        const std::unique_ptr<char[]> scratch(new (std::nothrow) char[sample_chunk_bytes]);
        if (!scratch || !begin(key.data(), pcm.size_bytes())) return false;
        const auto* bytes = reinterpret_cast<const char*>(pcm.data());
        for (unsigned offset = 0; offset < pcm.size_bytes(); offset += sample_chunk_bytes) {
            const unsigned count = std::min<unsigned>(sample_chunk_bytes, pcm.size_bytes() - offset);
            std::memcpy(scratch.get(), bytes + offset, count);
            if (!append({scratch.get(), count})) { abort(); return false; }
        }
        if (!commit()) { abort(); return false; }
    }
    return write_name(key.data(), {name.data(), std::strlen(name.data())});
}

bool SampleStore::reserve_recording()
{
    const std::lock_guard guard(mutex_);
    if (upload_ || recording_reserved_ || !available()) return false;
    recording_reserved_ = true;
    return true;
}

void SampleStore::release_recording()
{
    const std::lock_guard guard(mutex_);
    recording_reserved_ = false;
}
