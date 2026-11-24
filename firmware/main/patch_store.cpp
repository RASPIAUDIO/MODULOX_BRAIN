#include "patch_store.h"
#include "osc.h"
#include "sample_store.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string_view>

#include <dirent.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"

namespace {

constexpr char mount_path[] = "/patches";
constexpr char user_partition[] = "userpatches";
constexpr char user_namespace[] = "patches";
constexpr char tag[] = "patches";
using Document = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

class Lock {
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
private:
    SemaphoreHandle_t mutex_;
};

const cJSON* field(const cJSON* item, const char* key)
{
    return cJSON_GetObjectItemCaseSensitive(item, key);
}

bool is_string(const cJSON* item, const char* value)
{
    return cJSON_IsString(item) && std::strcmp(item->valuestring, value) == 0;
}

bool is_integer(const cJSON* item)
{
    return cJSON_IsNumber(item) && item->valuedouble == item->valueint;
}

bool valid_key(std::string_view key)
{
    return key.size() == user_key_length && key.front() == 'u' &&
           key.substr(1).find_first_not_of("0123456789abcdef") == std::string_view::npos;
}

void factory_key(char (&key)[user_key_length + 1], const char* filename)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char byte : std::string_view(filename)) {
        hash = (hash ^ byte) * 16777619u;
    }
    std::snprintf(key, sizeof(key), "f%08lx", static_cast<unsigned long>(hash));
}

bool read_parameters(const cJSON* parameters, const ModuleParameters& module,
                     std::span<int> values, bool is_oscillator = false)
{
    for (unsigned i = 0; i < module.parameters.size(); ++i) {
        values[i] = module.parameters[i].initial;
    }
    if (parameters && !cJSON_IsObject(parameters)) {
        return false;
    }
    uint64_t seen = 0;
    for (const cJSON* item = parameters ? parameters->child : nullptr; item; item = item->next) {
        if (!is_integer(item)) {
            return false;
        }
        const bool legacy_octave = is_oscillator && std::strcmp(item->string, "Octave") == 0;
        if (legacy_octave && (item->valueint < -2 || item->valueint > 2)) {
            return false;
        }
        const char* name = legacy_octave ? "Pitch" : item->string;
        const int value = legacy_octave ? item->valueint * Osc::semitones_per_octave : item->valueint;
        const auto match = std::find_if(module.parameters.begin(), module.parameters.end(),
            [name](const Parameter& parameter) { return std::strcmp(parameter.name, name) == 0; });
        if (match == module.parameters.end()) return false;
        const auto index = match - module.parameters.begin();
        if ((seen & (1ull << index)) || value < match->minimum || value > match->maximum) {
            return false;
        }
        seen |= 1ull << index;
        values[index] = value;
    }
    return true;
}

bool visit(AudioPlan& plan, unsigned slot, ModuleMask& ready, ModuleMask& visiting)
{
    const ModuleMask mask = module_bit(slot);
    if (visiting & mask) {
        return false;  // Feedback would require a delay module and a different execution plan.
    }
    if (ready & mask) {
        return true;
    }
    visiting |= mask;
    // Osc 2 is an audio-rate modulation source for Osc 1, even with no audible cable.
    if (slot == oscillator && plan.has(oscillator2) && !visit(plan, oscillator2, ready, visiting)) return false;
    for (int8_t input : plan.inputs[slot]) {
        if (input != no_module && !visit(plan, input, ready, visiting)) {
            return 0;
        }
    }
    visiting &= ~mask;
    ready |= mask;
    plan.order[plan.count++] = static_cast<int8_t>(slot);
    return true;
}

int8_t assign_slot(const cJSON* type, ModuleMask used)
{
    for (unsigned slot = 0; slot < max_patch_modules; ++slot) {
        if (!(used & (module_bit(slot))) && is_string(type, patch_module_types[slot])) {
            return slot;
        }
    }
    return no_module;
}

bool compile(const char* json, PatchStore::Modules modules, ModuleParameters settings,
             PreparedPatch& patch, PatchEntry* entry = nullptr)
{
    const Document root(cJSON_ParseWithOpts(json, nullptr, true), cJSON_Delete);
    const cJSON* name = field(root.get(), "name");
    const cJSON* version = field(root.get(), "version");
    const cJSON* nodes = field(root.get(), "modules");
    const cJSON* connections = field(root.get(), "connections");
    const cJSON* category = field(root.get(), "category");
    if (!cJSON_IsString(name) || !name->valuestring[0] ||
        std::strlen(name->valuestring) >= sizeof(PatchEntry::name) ||
        !is_integer(version) || (version->valueint < 1 || version->valueint > 3) ||
        !cJSON_IsArray(nodes) || !cJSON_IsArray(connections)) {
        return false;
    }

    const auto group = category && cJSON_IsString(category)
        ? patch_category(category->valuestring) : PatchCategory::user;
    if (category && (!cJSON_IsString(category) || group == PatchCategory::count)) return false;

    patch.plan = {};
    patch.waves.count = 0;
    patch.learn = {};
    if (!read_parameters(field(root.get(), "settings"), settings, patch.settings)) {
        return false;
    }
    const auto* bank = field(root.get(), "waves");
    if (bank && (!cJSON_IsArray(bank) || cJSON_GetArraySize(bank) > max_patch_waves)) {
        return false;
    }
    const cJSON* wave = nullptr;
    cJSON_ArrayForEach(wave, bank) {
        if (!cJSON_IsString(wave) || !valid_wave_key(wave->valuestring)) {
            return false;
        }
        auto& key = patch.waves.keys[patch.waves.count];
        std::strcpy(key.data(), wave->valuestring);
        if (std::find(patch.waves.keys.begin(), patch.waves.keys.begin() + patch.waves.count, key)
            != patch.waves.keys.begin() + patch.waves.count) {
            return false;
        }
        ++patch.waves.count;
    }
    AudioPlan& plan = patch.plan;
    plan.stereo_graph = version->valueint == 3;
    for (auto& inputs : plan.inputs) {
        inputs.fill(no_module);
    }
    std::array<const char*, max_patch_modules> ids{};
    const cJSON* node = nullptr;
    cJSON_ArrayForEach(node, nodes) {
        const cJSON* id = field(node, "id");
        const cJSON* type = field(node, "type");
        const int8_t slot = assign_slot(type, plan.modules);
        if (slot == no_module) {
            return false;
        }
        if (slot >= pan && !plan.stereo_graph) return false;
        plan.oscillators += is_string(type, "Osc");
        if (sample_slot(slot)) {
            const auto* sample = field(node, "sample");
            if (!cJSON_IsString(sample) || !valid_wave_key(sample->valuestring)) return false;
            const auto index = slot - sampler;
            plan.samplers += slot != granulizer;
            std::strcpy(patch.samples[index].data(), sample->valuestring);
            const auto* sample_name = field(node, "sampleName");
            auto& label = patch.sample_names[index];
            label.fill(0);
            if (cJSON_IsString(sample_name)) {
                for (unsigned i = 0; i < label.size() - 1 && sample_name->valuestring[i]; ++i) {
                    const unsigned char c = sample_name->valuestring[i];
                    label[i] = c >= 32 && c <= 126 ? c : '?';
                }
            }
        }
        plan.has_filter |= slot == filter;
        plan.has_envelope |= slot == envelope;
        if (ids[slot] || !cJSON_IsString(id) || !id->valuestring[0] ||
            std::strlen(id->valuestring) > 31 || is_string(id, "out") ||
            !read_parameters(field(node, "parameters"), modules[slot], patch.values[slot],
                             is_string(type, "Osc"))) {
            return false;
        }
        if (is_string(type, "Osc") && patch.values[slot][Osc::wave] > patch.waves.maximum()) {
            return false;
        }
        for (const char* existing : ids) {
            if (existing && is_string(id, existing)) {
                return false;
            }
        }
        ids[slot] = id->valuestring;
        plan.modules |= module_bit(slot);
    }
    const auto find = [&ids](const cJSON* id) -> int8_t {
        for (unsigned slot = 0; slot < ids.size(); ++slot) {
            if (ids[slot] && is_string(id, ids[slot])) {
                return static_cast<int8_t>(slot);
            }
        }
        return no_module;
    };

    const auto* cc_bindings = field(root.get(), "midiLearn");
    if (cc_bindings && !cJSON_IsArray(cc_bindings)) return false;
    const cJSON* binding = nullptr;
    cJSON_ArrayForEach(binding, cc_bindings) {
        if (!cJSON_IsArray(binding) || cJSON_GetArraySize(binding) != 3) return false;
        const auto* node = cJSON_GetArrayItem(binding, 0);
        const int slot = is_string(node, "$settings") ? int(max_patch_modules) : find(node);
        const auto* number = cJSON_GetArrayItem(binding, 2);
        if (slot < 0 || !is_integer(number) || number->valueint < 0 || number->valueint >= 120) return false;
        const auto params = slot == max_patch_modules ? settings.parameters : modules[slot].parameters;
        const auto* name = cJSON_GetArrayItem(binding, 1);
        const auto it = std::find_if(params.begin(), params.end(),
                                    [name](const Parameter& param) { return is_string(name, param.name); });
        if (it == params.end()) return false;
        auto& cc = patch.learn.cc[slot][it - params.begin()];
        if (cc >= 0) return false;
        cc = number->valueint;
    }

    const bool legacy = version->valueint == 1;
    const cJSON* edge = nullptr;
    cJSON_ArrayForEach(edge, connections) {
        if (!cJSON_IsArray(edge) || cJSON_GetArraySize(edge) != (legacy ? 2 : 3)) {
            return false;
        }
        const int8_t from = find(cJSON_GetArrayItem(edge, 0));
        const cJSON* target = cJSON_GetArrayItem(edge, 1);
        const bool to_output = is_string(target, "out");
        const int8_t to = find(target);
        if (!audio_source(from) || (!to_output && (!audio_source(to) || (!plan.stereo_graph && from == delay)))) {
            return false;
        }
        if (plan.stereo_graph && stereo_output(from) != (to_output || stereo_input(to))) return false;
        unsigned input = 0;
        if (!legacy) {
            const cJSON* port = cJSON_GetArrayItem(edge, 2);
            if (!is_integer(port) || port->valueint < 0) {
                return false;
            }
            input = port->valueint;
        } else if (to == mixer) {
            // Version 1 binds Mix gains to the oscillator order, regardless of edge order.
            if (from != oscillator && from != oscillator2 && from != oscillator3) {
                return false;
            }
            input = from == oscillator ? 0 : from - oscillator2 + 1;
        }
        if (to_output) {
            if (input != 0 || plan.output != no_module) {
                return false;
            }
            plan.output = from;
        } else {
            if (input >= patch_module_inputs[to] || plan.inputs[to][input] != no_module) {
                return false;
            }
            plan.inputs[to][input] = from;
        }
    }

    const cJSON* modulation = field(root.get(), "modulations");
    if (modulation && !cJSON_IsArray(modulation)) {
        return false;
    }
    ModuleMask control_contributors = 0;
    cJSON_ArrayForEach(edge, modulation) {
        if (!cJSON_IsArray(edge)) {
            return false;
        }
        const int8_t from = find(cJSON_GetArrayItem(edge, 0));
        const auto* target = cJSON_GetArrayItem(edge, 1);
        const int8_t to = is_string(target, "out") ? output_slot : find(target);
        const auto* parameter = cJSON_GetArrayItem(edge, 2);
        if (midi_source(from)) {
            if (cJSON_GetArraySize(edge) != 4 || to < 0 || to >= max_patch_modules) {
                return false;
            }
            unsigned port = 0;
            while (port < std::size(midi_ports) && !is_string(parameter, midi_ports[port])) ++port;
            if (port == std::size(midi_ports) || !midi_input(to, port) ||
                !is_string(cJSON_GetArrayItem(edge, 3), midi_ports[port]) ||
                plan.midi_to(to, port) ||
                ((to == env1 || to == env2) &&
                 (plan.midi_to(to, 2) || plan.midi_to(to, 3)))) {
                return false;
            }
            plan.midi_inputs[to] |= 1u << port;
            control_contributors |= module_bit(from);
            continue;
        }
        if (!modulation_source(from) || cJSON_GetArraySize(edge) != 3 ||
            ((to == disto || to == delay) && envelope_source(from))) {
            return false;
        }
        unsigned port = 0;
        while (port < std::size(modulation_inputs) &&
               (modulation_inputs[port].slot != to || !is_string(parameter, modulation_inputs[port].name))) {
            ++port;
        }
        if (port == std::size(modulation_inputs) || (plan.modulation & (1ull << port))) {
            return false;
        }
        plan.modulation |= 1ull << port;
        plan.sources[port] = from;
        control_contributors |= module_bit(from);
    }

    const cJSON* routing = field(root.get(), "routings");
    if (routing && !cJSON_IsArray(routing)) return false;
    cJSON_ArrayForEach(edge, routing) {
        if (!cJSON_IsArray(edge) || cJSON_GetArraySize(edge) != 4) return false;
        const int from = find(cJSON_GetArrayItem(edge, 0));
        const int to = find(cJSON_GetArrayItem(edge, 1));
        if (from < 0 || to < 0 || from == to) return false;
        unsigned input = 0;
        while (input < std::size(control_ports) && !is_string(cJSON_GetArrayItem(edge, 2), control_ports[input])) ++input;
        if (input == std::size(control_ports) || !control_input(to, input) ||
            plan.controls[to][input].source != no_module) return false;
        unsigned output = 0;
        if (input == cv_value) {
            if (!modulation_source(from) || !is_string(cJSON_GetArrayItem(edge, 3), "CV")) return false;
        } else {
            while (output < std::size(event_ports) && !is_string(cJSON_GetArrayItem(edge, 3), event_ports[output])) ++output;
            if (output == std::size(event_ports) || (!clock_module(from) && !(midi_source(from) && output == 0)) ||
                (input == control_run ? output != 2 : output == 2)) return false;
        }
        plan.controls[to][input] = {static_cast<int8_t>(from), static_cast<uint8_t>(output)};
        control_contributors |= (module_bit(from)) | (module_bit(to));
    }
    ModuleMask control_ready = 0, control_visiting = 0;
    const auto order_control = [&](auto&& self, int slot) -> bool {
        const ModuleMask mask = module_bit(slot);
        if (control_visiting & mask) return false;
        if (control_ready & mask) return true;
        control_visiting |= mask;
        const int source = plan.controls[slot][cv_value].source;
        if (utility_source(source) && !self(self, source)) return false;
        const bool per_voice = source >= 0 && plan.per_voice_source(source);
        if (utility_source(slot)) {
            plan.voice_controls[slot - sample_hold] = per_voice;
            plan.control_order[plan.control_count++] = slot;
        }
        control_visiting &= ~mask;
        control_ready |= mask;
        return true;
    };
    for (int slot : utility_slots) {
        if (plan.has(slot) && !order_control(order_control, slot)) return false;
    }
    for (unsigned port = 0; port < std::size(modulation_inputs); ++port) {
        if ((plan.modulation & (1ull << port)) && plan.per_voice_source(plan.sources[port]) &&
            (modulation_inputs[port].slot == delay || modulation_inputs[port].slot == disto)) return false;
    }

    ModuleMask ready = 0;
    ModuleMask visiting = 0;
    if (plan.has(oscillator) && patch.values[oscillator][Osc::fm] && !plan.has(oscillator2)) return false;
    if ((plan.modulation & (1ull << osc_fm)) && !plan.has(oscillator2)) return false;
    if (plan.output == no_module || (plan.oscillators == 0 && plan.samplers == 0 && !plan.has(granulizer) && !plan.has(fm4) &&
                                   !plan.has(noise) && !plan.has(sub)) ||
        !visit(plan, plan.output, ready, visiting) ||
        (ready | control_contributors) != plan.modules) {
        return false;
    }
    if (!configure_audio_plan(plan)) return false;
    for (unsigned port = 0; port < std::size(modulation_inputs); ++port) {
        const int slot = modulation_inputs[port].slot;
        if (slot < max_patch_modules && plan.shared[slot] && (plan.modulation & (1ull << port)) &&
            plan.per_voice_source(plan.sources[port])) return false;
    }
    for (unsigned slot = 0; slot < max_patch_modules; ++slot)
        if (plan.shared[slot] && plan.midi_inputs[slot]) return false;
    if (plan.has(ring_mod) && (plan.inputs[ring_mod][0] == no_module || plan.inputs[ring_mod][1] == no_module)) return false;
    for (const int8_t slot : {filter, envelope, mixer, disto, delay, vca, wavefolder, pan, pan2, pan3, chorus,
                              rotary, stereo_join, mono_down, stereo_mix}) {
        if (ids[slot] && std::all_of(plan.inputs[slot].begin(), plan.inputs[slot].end(),
                                    [](int8_t input) { return input == no_module; })) {
            return false;
        }
    }

    if (plan.output == envelope && plan.inputs[envelope][0] == filter) {
        if (plan.count == 3 && plan.inputs[filter][0] == oscillator) {
            plan.path = AudioPlan::Path::basic;
        } else if (plan.count == 6 && plan.inputs[filter][0] == mixer &&
                   plan.inputs[mixer] == AudioPlan::Inputs{oscillator, oscillator2, oscillator3}) {
            plan.path = AudioPlan::Path::triple;
        }
    }
    if (entry) {
        std::strcpy(entry->name, name->valuestring);
        entry->category = group;
    }
    return true;
}

}  // namespace

void PatchStore::mount(Modules modules, ModuleParameters settings, const WaveStore& waves, const SampleStore& samples)
{
    lock_ = xSemaphoreCreateMutexStatic(&lock_buffer_);
    settings_ = settings;
    waves_ = &waves;
    samples_ = &samples;
    std::unique_ptr<char[]> bytes(new (std::nothrow) char[max_patch_bytes + 1]);
    // Initializing NVS uses the main task's stack while this scratch patch is live.
    const std::unique_ptr<PreparedPatch> patch(new (std::nothrow) PreparedPatch);
    if (!bytes || !patch) {
        ESP_LOGE(tag, "Cannot allocate patch mount buffers");
        return;
    }
    const std::span buffer{bytes.get(), max_patch_bytes + 1};
    esp_vfs_spiffs_conf_t config{};
    config.base_path = mount_path;
    config.partition_label = "patches";
    config.max_files = 2;
    const esp_err_t result = esp_vfs_spiffs_register(&config);
    if (result == ESP_OK) {
        if (DIR* directory = opendir(mount_path)) {
            while (count_ < entries_.size()) {
                const dirent* entry = readdir(directory);
                if (!entry) {
                    break;
                }
                const std::string_view filename(entry->d_name);
                if (!filename.ends_with(".json") || filename.size() >= sizeof(PatchEntry::file) ||
                    filename.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") !=
                        std::string_view::npos) {
                    continue;
                }
                auto& record = entries_[count_];
                if (read_file(entry->d_name, buffer) >= 0 &&
                    compile(buffer.data(), modules, settings_, *patch, &record.entry)) {
                    std::strcpy(record.entry.file, entry->d_name);
                    ++count_;
                }
            }
            closedir(directory);
        }
    } else {
        ESP_LOGW(tag, "Cannot mount factory patches: %s", esp_err_to_name(result));
    }

    esp_err_t status = nvs_flash_init_partition(user_partition);
    if (status == ESP_OK) {
        status = nvs_open_from_partition(user_partition, user_namespace, NVS_READWRITE, &users_);
    }
    if (status == ESP_OK) {
        for (unsigned index = 0; index < count_;) {
            char key[user_key_length + 1];
            factory_key(key, entries_[index].entry.file);
            uint8_t deleted = 0;
            if (nvs_get_u8(users_, key, &deleted) == ESP_OK && deleted) {
                std::move(entries_.begin() + index + 1, entries_.begin() + count_, entries_.begin() + index);
                --count_;
            } else {
                ++index;
            }
        }
        for (unsigned index = 0; index < count_; ++index) {
            auto& record = entries_[index];
            factory_key(record.key, record.entry.file);
            PatchEntry overlay{};
            if (read_blob(record.key, buffer) >= 0 &&
                compile(buffer.data(), modules, settings_, *patch, &overlay)) {
                std::strcpy(record.entry.name, overlay.name);
                record.entry.category = overlay.category;
            } else {
                // Resolve the same key again on a manual save.
                record.key[0] = '\0';
            }
        }
        nvs_iterator_t iterator = nullptr;
        for (status = nvs_entry_find_in_handle(users_, NVS_TYPE_BLOB, &iterator);
             status == ESP_OK && count_ < entries_.size(); status = nvs_entry_next(&iterator)) {
            nvs_entry_info_t info{};
            if (nvs_entry_info(iterator, &info) != ESP_OK || !valid_key(info.key)) {
                continue;
            }
            auto& record = entries_[count_];
            if (read_blob(info.key, buffer) >= 0 &&
                compile(buffer.data(), modules, settings_, *patch, &record.entry)) {
                std::strcpy(record.key, info.key);
                std::snprintf(record.entry.file, sizeof(record.entry.file), "%s.json", info.key);
                ++count_;
            }
        }
        nvs_release_iterator(iterator);
    } else {
        ESP_LOGW(tag, "Cannot mount user patches: %s", esp_err_to_name(status));
    }

    std::sort(entries_.begin(), entries_.begin() + count_, [](const Record& a, const Record& b) {
        return std::strcmp(a.entry.file, b.entry.file) < 0;
    });
    for (unsigned index = 0; index < count_; ++index) entries_[index].revision = ++revision_clock_;
}

unsigned PatchStore::count() const
{
    const Lock guard(lock_);
    return count_;
}

bool PatchStore::entry(unsigned index, PatchEntry& result) const
{
    const Lock guard(lock_);
    if (index >= count_) {
        return false;
    }
    result = entries_[index].entry;
    return true;
}

PatchCategoryCounts PatchStore::category_counts() const
{
    const Lock guard(lock_);
    PatchCategoryCounts counts{};
    for (unsigned i = 0; i < count_; ++i) ++counts[static_cast<unsigned>(entries_[i].entry.category)];
    return counts;
}

int PatchStore::category_patch(PatchCategory category, unsigned position) const
{
    const Lock guard(lock_);
    for (unsigned i = 0; i < count_; ++i)
        if (entries_[i].entry.category == category && position-- == 0) return static_cast<int>(i);
    return -1;
}

int PatchStore::read_file(const char* filename, std::span<char> destination)
{
    char path[sizeof(mount_path) + sizeof(PatchEntry::file)];
    std::snprintf(path, sizeof(path), "%s/%s", mount_path, filename);
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return -1;
    }
    const auto size = std::fread(destination.data(), 1, destination.size(), file);
    const bool valid = size < destination.size() && std::feof(file) && !std::ferror(file) &&
                       std::memchr(destination.data(), '\0', size) == nullptr;
    std::fclose(file);
    if (!valid) {
        return -1;
    }
    destination[size] = '\0';
    return static_cast<int>(size);
}

int PatchStore::read_blob(const char* key, std::span<char> destination) const
{
    size_t size = destination.size() - 1;
    if (nvs_get_blob(users_, key, destination.data(), &size) != ESP_OK ||
        std::memchr(destination.data(), '\0', size)) {
        return -1;
    }
    destination[size] = '\0';
    return static_cast<int>(size);
}

int PatchStore::read(unsigned index, std::span<char> destination) const
{
    const Lock guard(lock_);
    if (index >= count_) {
        return -1;
    }
    return read_record(entries_[index], destination);
}

int PatchStore::read_record(const Record& record, std::span<char> destination) const
{
    return record.key[0] ? read_blob(record.key, destination)
                         : read_file(record.entry.file, destination);
}

bool PatchStore::prepare(unsigned index, Modules modules, PreparedPatch& patch) const
{
    std::unique_ptr<char[]> bytes(new (std::nothrow) char[max_patch_bytes + 1]);
    const Lock guard(lock_);
    if (!bytes || index >= count_ ||
        read_record(entries_[index], {bytes.get(), max_patch_bytes + 1}) < 0 ||
        !compile(bytes.get(), modules, settings_, patch)) {
        return false;
    }
    patch.revision = entries_[index].revision;
    return true;
}

bool PatchStore::sample_referenced(const char* key, bool& used) const
{
    used = false;
    if (!valid_wave_key(key)) return false;
    const std::unique_ptr<char[]> bytes(new (std::nothrow) char[max_patch_bytes + 1]);
    const Lock guard(lock_);
    if (!bytes) return false;
    for (unsigned index = 0; index < count_; ++index) {
        if (read_record(entries_[index], {bytes.get(), max_patch_bytes + 1}) < 0) return false;
        const Document root(cJSON_ParseWithOpts(bytes.get(), nullptr, true), cJSON_Delete);
        const auto* modules = root ? field(root.get(), "modules") : nullptr;
        if (!cJSON_IsArray(modules)) return false;
        const cJSON* node = nullptr;
        cJSON_ArrayForEach(node, modules) {
            const auto* sample = field(node, "sample");
            if (cJSON_IsString(sample) && std::strcmp(sample->valuestring, key) == 0) {
                used = true; return true;
            }
        }
    }
    return true;
}

bool PatchStore::referenced_waves(std::vector<WaveKey>& keys) const
{
    const Lock guard(lock_);
    std::unique_ptr<char[]> bytes(new (std::nothrow) char[max_patch_bytes + 1]);
    if (!bytes) {
        return false;
    }
    for (unsigned index = 0; index < count_; ++index) {
        if (read_record(entries_[index], {bytes.get(), max_patch_bytes + 1}) < 0) {
            return false;
        }
        const Document root(cJSON_ParseWithOpts(bytes.get(), nullptr, true), cJSON_Delete);
        if (!root) {
            return false;
        }
        const cJSON* wave = nullptr;
        cJSON_ArrayForEach(wave, field(root.get(), "waves")) {
            if (!cJSON_IsString(wave) || !valid_wave_key(wave->valuestring)) {
                return false;
            }
            WaveKey key{};
            std::strcpy(key.data(), wave->valuestring);
            if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
                keys.push_back(key);
            }
        }
    }
    return true;
}

int PatchStore::write_blob(const char* key, std::span<const char> bytes) const
{
    if (!users_) return storage;
    const auto result = nvs_set_blob(users_, key, bytes.data(), bytes.size());
    if (result != ESP_OK) return result == ESP_ERR_NVS_NOT_ENOUGH_SPACE ? full : storage;
    return nvs_commit(users_) == ESP_OK ? 0 : storage;
}

int PatchStore::save(const char* key, std::span<char> json, Modules modules)
{
    const bool factory = std::strlen(key) == 9 && key[0] == 'f' &&
        std::string_view(key + 1).find_first_not_of("0123456789abcdef") == std::string_view::npos;
    if ((!valid_key(key) && !factory) || json.empty() || json.size() > max_patch_bytes + 1 ||
        json.back() != '\0' || std::memchr(json.data(), '\0', json.size() - 1)) {
        return invalid;
    }
    const std::unique_ptr<PreparedPatch> patch(new (std::nothrow) PreparedPatch);
    if (!patch) return storage;
    PatchEntry entry{};
    if (!compile(json.data(), modules, settings_, *patch, &entry)) {
        return invalid;
    }
    for (unsigned i = 0; i < patch->waves.count; ++i) {
        if (!waves_->contains(patch->waves.keys[i].data())) {
            return invalid;
        }
    }
    for (unsigned i = 0; i < sample_slots.size(); ++i) {
        if (patch->plan.has(sample_slots[i]) && !samples_->size(patch->samples[i].data())) return invalid;
    }

    const Lock guard(lock_);
    if (!users_) {
        return storage;
    }
    unsigned index = 0;
    while (index < count_) {
        char candidate[user_key_length + 1];
        factory_key(candidate, entries_[index].entry.file);
        if (std::strcmp(entries_[index].key, key) == 0 || (factory && std::strcmp(candidate, key) == 0)) break;
        ++index;
    }
    if (factory && index == count_) return invalid;
    if (index == entries_.size()) {
        return full;
    }
    const int saved = write_blob(key, json.first(json.size() - 1));
    if (saved < 0) return saved;
    auto& record = entries_[index];
    std::strcpy(record.entry.name, entry.name);
    record.entry.category = entry.category;
    std::strcpy(record.key, key);
    if (!factory) std::snprintf(record.entry.file, sizeof(record.entry.file), "%s.json", key);
    record.revision = ++revision_clock_;
    if (index == count_) {
        ++count_;
    }
    return static_cast<int>(index);
}

bool PatchStore::remove(const char* filename)
{
    const Lock guard(lock_);
    const auto end = entries_.begin() + count_;
    const auto found = std::find_if(entries_.begin(), end, [filename](const Record& record) {
        return std::strcmp(record.entry.file, filename) == 0;
    });
    if (!users_ || found == end) return false;
    esp_err_t result;
    if (found->key[0] == 'u') {
        result = nvs_erase_key(users_, found->key);
    } else {
        char key[user_key_length + 1];
        factory_key(key, filename);
        result = nvs_set_u8(users_, key, 1);
    }
    if (result != ESP_OK || nvs_commit(users_) != ESP_OK) return false;
    std::move(found + 1, end, found);
    --count_;
    // Indices shifted. Invalidate every live-save token to prevent saving into another patch.
    for (unsigned i = 0; i < count_; ++i) entries_[i].revision = ++revision_clock_;
    return true;
}

namespace {

bool store_values(cJSON* parent, const char* key, const ModuleParameters& module)
{
    Document values(cJSON_CreateObject(), cJSON_Delete);
    if (!values) {
        return false;
    }
    for (const auto& parameter : module.parameters) {
        const int value = parameter.get();
        if (value != parameter.initial && !cJSON_AddNumberToObject(values.get(), parameter.name, value)) {
            return false;
        }
    }
    cJSON_DeleteItemFromObjectCaseSensitive(parent, key);
    if (!values->child) {
        return true;
    }
    if (!cJSON_AddItemToObject(parent, key, values.get())) {
        return false;
    }
    // Parent now owns the parameter object.
    values.release();
    return true;
}

bool store_learn(cJSON* bindings, const char* id, ModuleParameters module,
                 std::span<const int8_t> cc)
{
    for (unsigned i = 0; i < module.parameters.size(); ++i) {
        if (cc[i] < 0) continue;
        Document entry(cJSON_CreateArray(), cJSON_Delete);
        Document target(cJSON_CreateString(id), cJSON_Delete);
        Document param(cJSON_CreateString(module.parameters[i].name), cJSON_Delete);
        Document number(cJSON_CreateNumber(cc[i]), cJSON_Delete);
        if (!cJSON_AddItemToArray(entry.get(), target.get())) return false;
        target.release();
        if (!cJSON_AddItemToArray(entry.get(), param.get())) return false;
        param.release();
        if (!cJSON_AddItemToArray(entry.get(), number.get())) return false;
        number.release();
        if (!cJSON_AddItemToArray(bindings, entry.get())) return false;
        entry.release();
    }
    return true;
}

}  // namespace

int PatchStore::save_live(unsigned index, uint32_t& revision, Modules modules, const MidiLearnMap& learn,
                          const decltype(PreparedPatch::samples)& samples,
                          const decltype(PreparedPatch::sample_names)& sample_names)
{
    const Lock guard(lock_);
    if (!users_ || index >= count_) {
        return storage;
    }
    auto& record = entries_[index];
    if (record.revision != revision) {
        return changed;
    }
    std::unique_ptr<char[]> bytes(new (std::nothrow) char[max_patch_bytes + 6]);
    if (!bytes || read_record(record, {bytes.get(), max_patch_bytes + 1}) < 0) {
        return storage;
    }
    const Document root(cJSON_ParseWithOpts(bytes.get(), nullptr, true), cJSON_Delete);
    if (!root || !store_values(root.get(), "settings", settings_)) {
        return storage;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(root.get(), "midiLearn");
    auto* bindings = cJSON_AddArrayToObject(root.get(), "midiLearn");
    if (!bindings || !store_learn(bindings, "$settings", settings_, learn.cc[max_patch_modules])) {
        return storage;
    }
    ModuleMask used = 0;
    cJSON* node = nullptr;
    cJSON_ArrayForEach(node, field(root.get(), "modules")) {
        const cJSON* type = field(node, "type");
        const int8_t slot = assign_slot(type, used);
        const auto* id = field(node, "id");
        if (slot == no_module || !cJSON_IsString(id) || !store_values(node, "parameters", modules[slot]) ||
            !store_learn(bindings, id->valuestring, modules[slot], learn.cc[slot])) {
            return storage;
        }
        used |= module_bit(slot);
        if (sample_slot(slot)) {
            const auto at = slot - sampler;
            cJSON_DeleteItemFromObjectCaseSensitive(node, "sample");
            cJSON_DeleteItemFromObjectCaseSensitive(node, "sampleName");
            if (!cJSON_AddStringToObject(node, "sample", samples[at].data()) ||
                !cJSON_AddStringToObject(node, "sampleName", sample_names[at].data())) return storage;
        }
    }
    if (!bindings->child) cJSON_DeleteItemFromObjectCaseSensitive(root.get(), "midiLearn");
    if (!cJSON_PrintPreallocated(root.get(), bytes.get(), max_patch_bytes + 6, false)) {
        return storage;
    }
    const size_t size = std::strlen(bytes.get());
    char key[user_key_length + 1];
    if (record.key[0]) {
        std::strcpy(key, record.key);
    } else {
        factory_key(key, record.entry.file);
    }
    if (size > max_patch_bytes) return storage;
    const int saved = write_blob(key, {bytes.get(), size});
    if (saved < 0) return saved;
    std::strcpy(record.key, key);
    revision = record.revision = ++revision_clock_;
    return static_cast<int>(index);
}
