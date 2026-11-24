#pragma once

#include "waves.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include "mbedtls/sha256.h"

inline constexpr unsigned max_sample_bytes = 1u << 20;
inline constexpr unsigned sample_chunk_bytes = 4096;
inline constexpr unsigned sample_preview_points = 256;
struct SamplePeak { int16_t low = 0, high = 0; };
static_assert(sizeof(SamplePeak) == 4);

struct SampleData {
    std::unique_ptr<int16_t[], decltype(&std::free)> pcm{nullptr, std::free};
    unsigned frames = 0;
};

class SampleStore {
public:
    void mount();
    bool available() const;
    const char* status() const;
    unsigned size(const char* key) const;
    int read(const char* key, unsigned offset, std::span<char> bytes) const;
    bool load(const char* key, SampleData& sample) const;
    bool preview(const char* key, std::span<SamplePeak, sample_preview_points> peaks, unsigned& frames) const;
    bool remove(const char* key);
    bool writing() const;
    std::vector<WaveKey> keys() const;
    unsigned revision() const { return revision_.load(std::memory_order_acquire); }
    bool read_name(const char* key, std::span<char, 32> name) const;
    bool write_name(const char* key, std::span<const char> name);
    bool save_recording(std::span<const int16_t> pcm, WaveKey& key, std::span<char, 32> name);
    bool reserve_recording();
    void release_recording();

    // The USB task owns the upload. Other tasks see the file after commit.
    bool begin(const char* key, unsigned bytes);
    bool append(std::span<const char> bytes);
    bool commit();
    void abort();

private:
    mutable std::recursive_mutex mutex_;
    bool recording_reserved_ = false;
    bool mounted_ = false;
    const char* storage_status_ = "partition-missing";
    FILE* upload_ = nullptr;
    WaveKey key_{};
    unsigned remaining_ = 0;
    std::atomic<unsigned> revision_{0};
    mbedtls_sha256_context digest_{};
};
