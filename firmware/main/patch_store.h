#pragma once

#include "parameter.h"
#include "patch_plan.h"
#include "patch_settings.h"
#include "patch_category.h"
#include "waves.h"
class SampleStore;

#include <array>
#include <span>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

inline constexpr unsigned max_patch_bytes = 4096;
inline constexpr unsigned max_patches = 64;
inline constexpr unsigned user_key_length = 14;

struct PatchEntry {
    char file[32];
    char name[32];
    PatchCategory category = PatchCategory::user;
};

template<class T, unsigned Rows> struct ParameterRows {
    std::array<std::array<T, max_module_parameters>, Rows> standard{};
    std::array<T, fm4_parameter_count> fm{};
    std::span<T> operator[](unsigned slot) { return slot == fm4 ? std::span<T>{fm} : std::span<T>{standard[slot]}; }
    std::span<const T> operator[](unsigned slot) const { return slot == fm4 ? std::span<const T>{fm} : std::span<const T>{standard[slot]}; }
    constexpr unsigned size() const { return Rows; }
    void fill(T value) { for (auto& row : standard) row.fill(value); fm.fill(value); }
};

struct MidiLearnMap {
    ParameterRows<int8_t, max_patch_modules + 1> cc{};
    MidiLearnMap() { cc.fill(-1); }
};

struct PreparedPatch {
    AudioPlan plan;
    ParameterRows<int, max_patch_modules> values{};
    std::array<int, patch_setting_count> settings{};
    uint32_t revision = 0;
    WaveBank waves;
    std::array<WaveKey, sample_slots.size()> samples{};
    std::array<std::array<char, 32>, sample_slots.size()> sample_names{};
    MidiLearnMap learn;
};

class PatchStore {
public:
    using Modules = std::span<const ModuleParameters, max_patch_modules>;
    enum SaveError { invalid = -1, full = -2, storage = -3, changed = -4 };

    void mount(Modules modules, ModuleParameters settings, const WaveStore& waves, const SampleStore& samples);
    unsigned count() const;
    bool available() const { return users_ != 0; }
    bool entry(unsigned index, PatchEntry& result) const;
    PatchCategoryCounts category_counts() const;
    int category_patch(PatchCategory category, unsigned position) const;
    int read(unsigned index, std::span<char> destination) const;
    bool prepare(unsigned index, Modules modules, PreparedPatch& patch) const;
    int save(const char* key, std::span<char> json, Modules modules);
    bool remove(const char* filename);
    int save_live(unsigned index, uint32_t& revision, Modules modules, const MidiLearnMap& learn,
                  const decltype(PreparedPatch::samples)& samples,
                  const decltype(PreparedPatch::sample_names)& sample_names);
    bool referenced_waves(std::vector<WaveKey>& keys) const;
    bool sample_referenced(const char* key, bool& used) const;
    ModuleParameters settings() const { return settings_; }

private:
    struct Record {
        PatchEntry entry;
        char key[user_key_length + 1];
        uint32_t revision;
    };
    static int read_file(const char* filename, std::span<char> destination);
    int read_blob(const char* key, std::span<char> destination) const;
    int read_record(const Record& record, std::span<char> destination) const;
    int write_blob(const char* key, std::span<const char> bytes) const;
    std::array<Record, max_patches> entries_{};
    unsigned count_ = 0;
    uint32_t revision_clock_ = 0;
    nvs_handle_t users_ = 0;
    mutable StaticSemaphore_t lock_buffer_{};
    SemaphoreHandle_t lock_ = nullptr;
    ModuleParameters settings_{};
    const WaveStore* waves_ = nullptr;
    const SampleStore* samples_ = nullptr;
};
