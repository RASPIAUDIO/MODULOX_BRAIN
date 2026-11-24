#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "nvs.h"

inline constexpr int builtin_waves = 3;
inline constexpr unsigned max_patch_waves = 125;
inline constexpr unsigned wave_samples = 1024;
inline constexpr unsigned wave_key_size = 14;
using WaveKey = std::array<char, wave_key_size + 1>;

struct WaveData {
    char name[32];
    int16_t pcm[wave_samples];
};
static_assert(sizeof(WaveData) == 2080);

struct WaveBank {
    std::array<WaveKey, max_patch_waves> keys{};
    unsigned count = 0;
    int maximum() const { return builtin_waves + count - 1; }
    const WaveKey& key(int wave) const { return keys[wave - builtin_waves]; }
};

bool valid_wave_key(std::string_view key);

class WaveStore {
public:
    enum Result { ok, invalid, storage, full };

    void mount();
    bool available() const { return handle_ != 0; }
    bool contains(const char* key) const;
    bool read(const char* key, WaveData& wave) const;
    Result write(const char* key, std::span<const char> bytes);
    bool prune(std::span<const WaveKey> referenced);

private:
    nvs_handle_t handle_ = 0;
};
