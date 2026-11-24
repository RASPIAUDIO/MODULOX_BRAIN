#include "patch_usb.h"
#include "controls.h"
#include "display.h"
#include "runtime_diagnostics.h"
#include "sample_store.h"
#include "firmware_update.h"

#include <charconv>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>
#include <string>
#include <utility>

#include "cJSON.h"
#include "freertos/task.h"
#include "tusb.h"

namespace {

char command[160];
unsigned command_length = 0;
bool command_overflow = false;
constexpr unsigned header_space = 16;
alignas(4) char response[header_space + 20480 + 1];
unsigned response_length = 0;
unsigned sent = 0;
char upload_key[user_key_length + 1];
WaveKey wave_upload_key;
enum class Upload { patch, wave, sample_chunk, sample_name, firmware_chunk };
unsigned firmware_offset = 0;
Upload upload_kind = Upload::patch;
unsigned upload_size = 0;
unsigned uploaded = 0;
TickType_t last_upload_byte = 0;

void reply(const char* message)
{
    sent = 0;
    response_length = std::strlen(message);
    std::memcpy(response, message, response_length);
}

void send_body(unsigned size)
{
    const int header = std::snprintf(response, header_space, "OK %u\n", size);
    std::memmove(response + header, response + header_space, size);
    response_length = header + size;
    sent = 0;
}

bool parse_number(std::string_view value, unsigned& number)
{
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    return error == std::errc{} && end == value.data() + value.size();
}

bool describe_parameters(cJSON* parent, ModuleParameters module)
{
    cJSON* parameters = cJSON_AddArrayToObject(parent, "parameters");
    if (!parameters) {
        return false;
    }
    for (const auto& parameter : module.parameters) {
        cJSON* item = cJSON_CreateObject();
        if (!cJSON_AddItemToArray(parameters, item)) {
            cJSON_Delete(item);
            return false;
        }
        if (!cJSON_AddStringToObject(item, "name", parameter.name) ||
            !cJSON_AddNumberToObject(item, "min", parameter.minimum) ||
            !cJSON_AddNumberToObject(item, "max", parameter.maximum) ||
            !cJSON_AddNumberToObject(item, "step", parameter.step) ||
            !cJSON_AddNumberToObject(item, "initial", parameter.initial) ||
            !cJSON_AddStringToObject(item, "unit", parameter.unit)) {
            return false;
        }
        if (!parameter.choices.empty()) {
            cJSON* choices = cJSON_AddArrayToObject(item, "choices");
            if (!choices) {
                return false;
            }
            for (const char* choice : parameter.choices) {
                cJSON* label = cJSON_CreateString(choice);
                if (!cJSON_AddItemToArray(choices, label)) {
                    cJSON_Delete(label);
                    return false;
                }
            }
        }
    }
    return true;
}

bool describe_modulation(cJSON* parent, int slot)
{
    cJSON* ports = cJSON_AddArrayToObject(parent, "modulationInputs");
    if (!ports) {
        return false;
    }
    for (const auto& port : modulation_inputs) {
        if (port.slot == slot) {
            cJSON* label = cJSON_CreateString(port.name);
            if (!cJSON_AddItemToArray(ports, label)) {
                cJSON_Delete(label);
                return false;
            }
        }
    }
    return true;
}

bool describe_midi_ports(cJSON* type, int slot)
{
    cJSON* ports = cJSON_AddArrayToObject(type, midi_source(slot) ? "midiOutputs" : "midiInputs");
    if (!ports) return false;
    for (unsigned i = 0; i < std::size(midi_ports); ++i) {
        if (!midi_source(slot) && !midi_input(slot, i)) continue;
        cJSON* item = cJSON_CreateString(midi_ports[i]);
        if (!cJSON_AddItemToArray(ports, item)) {
            cJSON_Delete(item);
            return false;
        }
    }
    return true;
}

bool describe_control_ports(cJSON* type, int slot)
{
    auto* cv = cJSON_AddArrayToObject(type, "cvInputs");
    auto* triggers = cJSON_AddArrayToObject(type, "triggerInputs");
    auto* outputs = cJSON_AddArrayToObject(type, "triggerOutputs");
    if (!cv || !triggers || !outputs) return false;
    for (unsigned input = 0; input < std::size(control_ports); ++input) {
        if (!control_input(slot, input)) continue;
        auto* name = cJSON_CreateString(control_ports[input]);
        if (!cJSON_AddItemToArray(input == cv_value ? cv : triggers, name)) {
            cJSON_Delete(name);
            return false;
        }
    }
    if (clock_module(slot)) {
        for (const char* output : event_ports) {
            auto* name = cJSON_CreateString(output);
            if (!cJSON_AddItemToArray(outputs, name)) {
                cJSON_Delete(name);
                return false;
            }
        }
    }
    return true;
}

bool build_catalog(PatchStore::Modules modules, ModuleParameters settings, const SampleStore& samples)
{
    const std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    cJSON* types = cJSON_AddArrayToObject(root.get(), "modules");
    cJSON* options = cJSON_AddObjectToObject(root.get(), "settings");
    cJSON* output = cJSON_AddObjectToObject(root.get(), "output");
    cJSON* firmware = cJSON_AddObjectToObject(root.get(), "firmware");
    if (!firmware || !firmware_info(firmware)) return false;
    if (display::mirror_available()) {
        auto* screen = cJSON_AddObjectToObject(root.get(), "screenMirror");
        if (!screen || !cJSON_AddNumberToObject(screen, "width", display::width) ||
            !cJSON_AddNumberToObject(screen, "height", display::height) ||
            !cJSON_AddNumberToObject(screen, "bandHeight", display::mirror_band_height)) return false;
    }
    if (!types || !options || !output || !describe_parameters(options, settings) ||
        !describe_modulation(output, output_slot) ||
        !cJSON_AddBoolToObject(root.get(), "midiLearn", true) ||
        !cJSON_AddBoolToObject(root.get(), "patchManagement", true) ||
        !cJSON_AddBoolToObject(root.get(), "patchCategories", true) ||
        !cJSON_AddBoolToObject(root.get(), "clockRouting", true) ||
          !cJSON_AddBoolToObject(root.get(), "vaExpansion", true) ||
        !cJSON_AddBoolToObject(root.get(), "stereoGraph", true) ||
        !cJSON_AddBoolToObject(root.get(), "fm4", true) ||
        !cJSON_AddNumberToObject(output, "inputChannels", 2) ||
        !cJSON_AddNumberToObject(root.get(), "maxPatches", max_patches) ||
        !cJSON_AddNumberToObject(root.get(), "maxBytes", max_patch_bytes) ||
        !cJSON_AddNumberToObject(root.get(), "maxWaves", max_patch_waves)) {
        return false;
    }
    if (!cJSON_AddStringToObject(root.get(), "sampleStatus", samples.status()) ||
        !cJSON_AddBoolToObject(root.get(), "sampleNames", true) ||
        !cJSON_AddBoolToObject(root.get(), "sampleLibrary", true) ||
        !cJSON_AddBoolToObject(root.get(), "samplePreview", true) ||
        !cJSON_AddBoolToObject(root.get(), "sampleDeletion", true) ||
        !cJSON_AddBoolToObject(root.get(), "audioRecording", true) ||
        !cJSON_AddNumberToObject(root.get(), "maxRecordingSeconds", 5) ||
        (samples.available() && !cJSON_AddNumberToObject(root.get(), "maxSampleBytes", max_sample_bytes))) return false;
    for (const auto slot : {oscillator, filter, envelope, mixer, lfo, env1, midi, disto, delay, sampler, granulizer,
                             clock1, arp, sample_hold, random1, gain_offset, slew, noise, sub, vca, ring_mod, wavefolder,
                             pan, chorus, rotary, stereo_join, mono_down, stereo_mix, fm4}) {
        cJSON* type = cJSON_CreateObject();
        if (!cJSON_AddItemToArray(types, type)) {
            cJSON_Delete(type);
            return false;
        }
        if (!cJSON_AddStringToObject(type, "type", patch_module_types[slot]) ||
            !cJSON_AddNumberToObject(type, "limit", module_limit(slot)) ||
            !cJSON_AddNumberToObject(type, "inputs", patch_module_inputs[slot]) ||
            !cJSON_AddNumberToObject(type, "inputChannels", stereo_input(slot) ? 2 : 1) ||
            !cJSON_AddNumberToObject(type, "outputChannels", stereo_output(slot) ? 2 : 1) ||
            !describe_parameters(type, modules[slot]) || !describe_modulation(type, slot) ||
            !describe_midi_ports(type, slot) || !describe_control_ports(type, slot) ||
            (!audio_source(slot) && !cJSON_AddBoolToObject(type, "controlOutput", true)) ||
            (shared_effect(slot) && !cJSON_AddBoolToObject(type, "sharedEffect", true))) {
            return false;
        }
        if (slot == stereo_join) {
            auto* labels = cJSON_AddArrayToObject(type, "inputLabels");
            if (!labels) return false;
            for (const char* name : {"L / Mono", "R"}) {
                auto* label = cJSON_CreateString(name);
                if (!cJSON_AddItemToArray(labels, label)) { cJSON_Delete(label); return false; }
            }
        }
    }
    char* body = response + header_space;
    if (!cJSON_PrintPreallocated(root.get(), body, sizeof(response) - header_space, false)) {
        return false;
    }
    send_body(std::strlen(body));
    return true;
}

void begin_upload(unsigned bytes, Upload kind)
{
    upload_size = bytes;
    upload_kind = kind;
    uploaded = 0;
    last_upload_byte = xTaskGetTickCount();
    reply("READY\n");
}

void make_response(PatchStore& patches, PatchStore::Modules modules, const WaveStore& waves, SampleStore& samples,
                   bool (*prune_waves)(), SampleDeleteStatus (*delete_sample)(const char*), std::string_view line)
{
    sent = 0;
    const auto update_reply = [](const char* error) {
        if (!error) reply("OK 0\n");
        else { response_length = std::snprintf(response, sizeof(response), "ERR %s\n", error); sent = 0; }
    };
    if (line == "FWINFO") {
        const std::unique_ptr<cJSON, decltype(&cJSON_Delete)> info(cJSON_CreateObject(), cJSON_Delete);
        if (!info || !firmware_info(info.get()) || !cJSON_PrintPreallocated(info.get(), response + header_space, sizeof(response) - header_space, false)) reply("ERR firmware-storage\n");
        else send_body(std::strlen(response + header_space));
        return;
    }
    if (line == "FWABORT") { firmware_update_abort(); reply("OK 0\n"); return; }
    if (line == "FWEND") { update_reply(firmware_update_finish()); return; }
    if (line == "FWROLLBACK") { update_reply(firmware_request_rollback()); return; }
    if (line.starts_with("FWBEGIN ")) {
        const auto args = line.substr(8); const auto split = args.find(' '); unsigned size;
        if (samples.writing()) { reply("ERR sample-busy\n"); return; }
        if (split == std::string_view::npos || !parse_number(args.substr(0, split), size) || args.size() - split - 1 != 64) { reply("ERR firmware-invalid\n"); return; }
        char hash[65]{}; std::memcpy(hash, args.data() + split + 1, 64);
        update_reply(firmware_update_begin(size, hash)); return;
    }
    if (line.starts_with("FWCHUNK ")) {
        const auto args = line.substr(8); const auto split = args.find(' '); unsigned size;
        if (!firmware_update_active() || split == std::string_view::npos || !parse_number(args.substr(0, split), firmware_offset) ||
            !parse_number(args.substr(split + 1), size) || !size || size > 4096) { reply("ERR firmware-invalid\n"); return; }
        begin_upload(size, Upload::firmware_chunk); return;
    }
    if (firmware_update_active()) { reply("ERR firmware-busy\n"); return; }
    if (line.starts_with("SCREEN ")) {
        const auto args = line.substr(7);
        const auto split = args.find(' ');
        unsigned since = 0, first_band = 0;
        if (split == std::string_view::npos || !parse_number(args.substr(0, split), since) ||
            !parse_number(args.substr(split + 1), first_band)) { reply("ERR screen-invalid\n"); return; }
        const unsigned size = display::read_mirror(since, first_band, std::span{response}.subspan(header_space));
        if (size) send_body(size);
        else reply("ERR screen-invalid\n");
        return;
    }
    if (line.starts_with("UIKEY ")) {
        using enum PanelInput;
        constexpr std::pair<std::string_view, PanelInput> keys[] = {
            {"left", left}, {"center", center}, {"right", right}, {"menu", menu},
            {"push", encoder_press}, {"cw", clockwise}, {"ccw", counterclockwise},
        };
        const auto key = line.substr(6);
        for (const auto& [name, input] : keys) {
            if (key != name) continue;
            reply(display::mirror_available() && post_control_input(input) ? "OK 0\n" : "ERR screen-busy\n");
            return;
        }
        reply("ERR screen-invalid\n");
        return;
    }
    if (line.starts_with("DELETE ")) {
        const std::string filename(line.substr(7));
        reply(patches.remove(filename.c_str()) ? "OK 0\n" : "ERR storage\n");
        return;
    }
    if (line == "DIAG") {
        response_length = format_runtime_diagnostics(response, sizeof(response));
        return;
    }
    if (line == "LIST") {
        const unsigned count = patches.count();
        response_length = std::snprintf(response, sizeof(response), "OK %u\n", count);
        for (unsigned index = 0; index < count; ++index) {
            PatchEntry entry;
            if (!patches.entry(index, entry)) {
                reply("ERR\n");
                return;
            }
            response_length += std::snprintf(response + response_length,
                sizeof(response) - response_length, "%u %s\n", index, entry.file);
        }
        return;
    }
    if (line == "CATALOG") {
        if (!build_catalog(modules, patches.settings(), samples)) {
            reply("ERR\n");
        }
        return;
    }
    if (line.starts_with("SDELETE ") || line.starts_with("SPREVIEW ")) {
        const auto id = line.substr(line.find(' ') + 1);
        if (!valid_wave_key(id)) { reply("ERR sample-invalid\n"); return; }
        WaveKey key{}; std::memcpy(key.data(), id.data(), id.size());
        if (line.starts_with("SDELETE ")) {
            const auto result = delete_sample ? delete_sample(key.data()) : SampleDeleteStatus::error;
            switch (result) {
                case SampleDeleteStatus::deleted: reply("OK 0\n"); break;
                case SampleDeleteStatus::in_use: reply("ERR sample-in-use\n"); break;
                case SampleDeleteStatus::busy: reply("ERR sample-busy\n"); break;
                default: reply("ERR sample-storage\n"); break;
            }
        } else {
            unsigned frames = 0;
            auto* peaks = reinterpret_cast<SamplePeak*>(response + header_space + 4);
            if (!samples.preview(key.data(), std::span<SamplePeak, sample_preview_points>{peaks, sample_preview_points}, frames)) {
                reply("ERR sample-missing\n"); return;
            }
            const uint32_t length = frames;
            std::memcpy(response + header_space, &length, sizeof(length));
            send_body(4 + sample_preview_points * sizeof(SamplePeak));
        }
        return;
    }
    if (line.starts_with("SLIST ")) {
        unsigned offset = 0;
        if (!parse_number(line.substr(6), offset)) { reply("ERR sample-invalid\n"); return; }
        const auto keys = samples.keys();
        if (offset > keys.size()) { reply("ERR sample-invalid\n"); return; }
        const std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
        auto* items = root ? cJSON_AddArrayToObject(root.get(), "items") : nullptr;
        if (!items) { reply("ERR sample-storage\n"); return; }
        const unsigned end = std::min<unsigned>(keys.size(), offset + 32);
        for (unsigned i = offset; i < end; ++i) {
            std::array<char, 32> name{};
            if (!samples.read_name(keys[i].data(), name)) std::strcpy(name.data(), keys[i].data());
            auto* item = cJSON_CreateObject();
            if (!cJSON_AddItemToArray(items, item)) { cJSON_Delete(item); reply("ERR sample-storage\n"); return; }
            if (!cJSON_AddStringToObject(item, "id", keys[i].data()) ||
                !cJSON_AddStringToObject(item, "name", name.data()) ||
                !cJSON_AddNumberToObject(item, "bytes", samples.size(keys[i].data()))) { reply("ERR sample-storage\n"); return; }
        }
        if ((end < keys.size() ? !cJSON_AddNumberToObject(root.get(), "next", end) :
                !cJSON_AddNullToObject(root.get(), "next")) ||
            !cJSON_PrintPreallocated(root.get(), response + header_space, sizeof(response) - header_space, false)) {
            reply("ERR sample-storage\n"); return;
        }
        send_body(std::strlen(response + header_space));
        return;
    }
    if (line == "SCOMMIT") {
        const bool result = samples.commit();
        if (!result) samples.abort();
        reply(result ? "OK 0\n" : "ERR sample-storage\n");
        return;
    }
    if (line.starts_with("SCHUNK ")) {
        unsigned bytes = 0;
        if (parse_number(line.substr(7), bytes) && bytes && bytes <= sample_chunk_bytes) {
            begin_upload(bytes, Upload::sample_chunk);
            return;
        }
    }
    if (line.starts_with("SHAS ") || line.starts_with("SBEGIN ") || line.starts_with("SGET ") ||
        line.starts_with("SNAME ")) {
        const auto args = line.substr(line.find(' ') + 1);
        const auto id = args.substr(0, wave_key_size);
        if (!valid_wave_key(id)) {
            reply("ERR sample-invalid\n");
            return;
        }
        WaveKey key{};
        std::memcpy(key.data(), id.data(), id.size());
        if (line.starts_with("SHAS ") && args.size() == wave_key_size) {
            reply(samples.size(key.data()) ? "OK 1\n" : "OK 0\n");
            return;
        }
        unsigned number = 0;
        if (args.size() > wave_key_size + 1 && args[wave_key_size] == ' ' &&
            parse_number(args.substr(wave_key_size + 1), number)) {
            if (line.starts_with("SBEGIN ")) {
                reply(samples.begin(key.data(), number) ? "OK 0\n" : "ERR sample-storage\n");
                return;
            }
            if (line.starts_with("SGET ")) {
                const int count = samples.read(key.data(), number, {response + header_space, sample_chunk_bytes});
                if (count >= 0) send_body(count);
                else reply("ERR sample-missing\n");
                return;
            }
            if (line.starts_with("SNAME ") && number && number <= 31) {
                wave_upload_key = key;
                begin_upload(number, Upload::sample_name);
                return;
            }
        }
        reply("ERR sample-invalid\n");
        return;
    }
    if (line == "WGC") {
        reply(prune_waves() ? "OK 0\n" : "ERR wave-storage\n");
        return;
    }
    if (line.starts_with("WHAS ") || line.starts_with("WGET ") || line.starts_with("WPUT ")) {
        const auto key = line.substr(5);
        if (!valid_wave_key(key)) {
            reply("ERR wave-invalid\n");
            return;
        }
        WaveKey cache_key{};
        std::memcpy(cache_key.data(), key.data(), key.size());
        if (line.starts_with("WHAS")) {
            reply(waves.contains(cache_key.data()) ? "OK 1\n" : "OK 0\n");
        } else if (line.starts_with("WGET")) {
            WaveData data;
            if (!waves.read(cache_key.data(), data)) {
                reply("ERR wave-missing\n");
                return;
            }
            std::memcpy(response + header_space, &data, sizeof(data));
            send_body(sizeof(data));
        } else {
            wave_upload_key = cache_key;
            begin_upload(sizeof(WaveData), Upload::wave);
        }
        return;
    }
    if (line.starts_with("GET ")) {
        unsigned index = 0;
        if (parse_number(line.substr(4), index)) {
            const int size = patches.read(index, {response + header_space,
                                                  max_patch_bytes + 1});
            if (size >= 0) {
                send_body(size);
                return;
            }
        }
    }
    if (line.starts_with("PUT ") && line.find(' ', 4) != std::string_view::npos) {
        const auto separator = line.find(' ', 4);
        const std::string_view key = line.substr(4, separator - 4);
        unsigned size = 0;
        if (((key.size() == user_key_length && key.front() == 'u') ||
             (key.size() == 9 && key.front() == 'f')) && key.substr(1).find_first_not_of("0123456789abcdef") ==
                                     std::string_view::npos &&
            parse_number(line.substr(separator + 1), size) &&
            size > 0 && size <= max_patch_bytes) {
            std::memcpy(upload_key, key.data(), key.size());
            upload_key[key.size()] = '\0';
            begin_upload(size, Upload::patch);
            return;
        }
    }
    reply("ERR\n");
}

void receive_upload(PatchStore& patches, PatchStore::Modules modules, WaveStore& waves, SampleStore& samples)
{
    const auto now = xTaskGetTickCount();
    if (now - last_upload_byte >= pdMS_TO_TICKS(3000)) {
        tud_cdc_read_flush();
        upload_size = 0;
        samples.abort();
        firmware_update_abort();
        reply("ERR timeout\n");
        return;
    }
    const auto received = tud_cdc_read(response + uploaded, upload_size - uploaded);
    if (received) {
        last_upload_byte = now;
        uploaded += received;
    }
    if (uploaded != upload_size) {
        return;
    }
    if (upload_kind == Upload::firmware_chunk) {
        const char* error = firmware_update_write(firmware_offset, {response, upload_size});
        upload_size = 0;
        if (!error) reply("OK 0\n");
        else { response_length = std::snprintf(response, sizeof(response), "ERR %s\n", error); sent = 0; }
        return;
    }
    if (upload_kind == Upload::sample_name) {
        const bool result = samples.write_name(wave_upload_key.data(), {response, upload_size});
        upload_size = 0;
        reply(result ? "OK 0\n" : "ERR sample-storage\n");
        return;
    }
    if (upload_kind == Upload::sample_chunk) {
        const bool result = samples.append({response, upload_size});
        if (!result) samples.abort();
        upload_size = 0;
        reply(result ? "OK 0\n" : "ERR sample-storage\n");
        return;
    }
    if (upload_kind == Upload::wave) {
        const auto result = waves.write(wave_upload_key.data(), {response, upload_size});
        upload_size = 0;
        switch (result) {
            case WaveStore::ok: reply("OK 0\n"); break;
            case WaveStore::full: reply("ERR wave-full\n"); break;
            case WaveStore::invalid: reply("ERR wave-invalid\n"); break;
            case WaveStore::storage: reply("ERR wave-storage\n"); break;
        }
        return;
    }
    response[upload_size] = '\0';
    const int index = patches.save(upload_key, {response, upload_size + 1}, modules);
    upload_size = 0;
    if (index >= 0) {
        sent = 0;
        response_length = std::snprintf(response, sizeof(response), "OK %d\n", index);
    } else if (index == PatchStore::invalid) {
        reply("ERR invalid\n");
    } else if (index == PatchStore::full) {
        reply("ERR full\n");
    } else {
        reply("ERR storage\n");
    }
}

}  // namespace

void service_patch_usb(PatchStore& patches, PatchStore::Modules modules, WaveStore& waves, SampleStore& samples,
                       bool (*prune_waves)(), bool recording, SampleDeleteStatus (*delete_sample)(const char*))
{
    firmware_update_service();
    if (!tud_cdc_connected()) {
        samples.abort();
        firmware_update_abort();
        command_length = response_length = sent = upload_size = 0;
        command_overflow = false;
        return;
    }
    if (sent < response_length) {
        sent += tud_cdc_write(response + sent, response_length - sent);
        tud_cdc_write_flush();
        return;
    }
    if (upload_size) {
        receive_upload(patches, modules, waves, samples);
        return;
    }
    for (unsigned i = 0; i < sizeof(command); ++i) {
        const int byte = tud_cdc_read_char();
        if (byte < 0) {
            break;
        }
        if (byte == '\n') {
            const auto line = command_overflow ? std::string_view{} : std::string_view{command, command_length};
            // The virtual panel also needs to show and stop an ongoing recording.
            if (recording && !line.starts_with("SCREEN ") && !line.starts_with("UIKEY ")) reply("ERR recording-busy\n");
            else make_response(patches, modules, waves, samples, prune_waves, delete_sample, line);
            command_length = 0;
            command_overflow = false;
            break;
        }
        if (byte != '\r') {
            if (command_length == sizeof(command)) {
                command_overflow = true;
            } else {
                command[command_length++] = static_cast<char>(byte);
            }
        }
    }
}

bool patch_usb_transfer_active()
{
    return upload_size != 0 || firmware_update_active();
}

