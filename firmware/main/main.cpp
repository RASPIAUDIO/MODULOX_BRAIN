#include "audio_output.h"
#include "controls.h"
#include "patch_store.h"
#include "patch_usb.h"
#include "sample_store.h"
#include "runtime_diagnostics.h"
#include "resource_info.h"
#include "recording.h"
#include "synth_voice.h"
#include "firmware_update.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <new>

#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "hal/usb_serial_jtag_ll.h"
#include "hal/usb_wrap_ll.h"
#include "freertos/queue.h"
#include "tinyusb.h"
#include "tusb.h"
#ifdef MODULOX_USB_DIAGNOSTICS
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "soc/usb_wrap_reg.h"
#ifndef SOC_DPORT_USB_BASE
#define SOC_DPORT_USB_BASE 0x60080000UL
#endif
#include "soc/usb_reg.h"
#endif

namespace {

constexpr unsigned block_frames = SynthVoice::block_frames;
constexpr unsigned load_window_blocks = SynthVoice::sample_rate / block_frames;
constexpr uint32_t cycles_per_tenth_percent =
    CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1'000'000ULL * load_window_blocks * block_frames /
    (1000 * SynthVoice::sample_rate);
std::atomic<unsigned> audio_load_tenths{0}, audio_peak_tenths{0}, audio_overruns{0};
std::atomic<bool> audio_load_valid{false};
std::atomic<bool> input_overflow{false};
PatchSettings settings;
SynthVoice voice{settings};  // Static, writable data in internal DRAM.
PatchStore patches;
WaveStore wave_store;
SampleStore sample_store;
Recording recording;
SampleData recording_data, library_sample;
bool recording_prepared = false;
WaveKey library_key{};
std::array<char, 32> library_name{};
unsigned library_revision = 0;
std::array<SampleData, sample_slots.size()> active_samples;
decltype(PreparedPatch::samples) active_sample_keys;
decltype(PreparedPatch::sample_names) active_sample_names;
std::recursive_mutex wave_mutex;
WaveBank active_bank;
std::array<WaveData, oscillator_slots.size()> staged_waves;
std::array<WaveKey, oscillator_slots.size()> loaded_waves;
char wave_names[oscillator_slots.size()][sizeof(WaveData::name)]{};
uint32_t active_revision;
MidiLearnMap midi_learn;

constexpr char tag[] = "modulox";
constexpr unsigned queue_capacity = 32;
// One display request may be pending, without reducing the 32 MIDI/control slots.
constexpr unsigned command_queue_capacity = queue_capacity + 1;
constexpr unsigned usb_interface_count = 4;
constexpr unsigned usb_config_length = TUD_CONFIG_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_CDC_DESC_LEN;

const uint8_t usb_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, usb_interface_count, 0, usb_config_length, 0, 100),
    TUD_MIDI_DESCRIPTOR(0, 4, 0x01, 0x81, 64),
    TUD_CDC_DESCRIPTOR(2, 5, 0x82, 8, 0x03, 0x83, 64),
};
constexpr char language[] = {0x09, 0x04, '\0'};
const char* usb_strings[] = {
    language, "Modulox", "Modulox Sine", "", "Modulox Sine MIDI", "Modulox Patches",
};

enum class MidiAction : uint8_t { note_on, note_off, all_notes_off, all_sound_off, disconnect, pause, pause_wave, resume, play_sample, filter_response };
enum MidiSource : uint8_t { usb, physical };

struct MidiEvent {
    MidiAction action;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    TaskHandle_t reply = nullptr;
    const AudioPlan* plan = nullptr;
    MidiSource source = usb;
};

QueueHandle_t midi_events;
QueueHandle_t clock_events;

void IRAM_ATTR sync_edge(void*)
{
    const ClockEvent event{static_cast<uint64_t>(esp_timer_get_time()), 3, 0xf8};
    BaseType_t awakened = pdFALSE;
    xQueueSendFromISR(clock_events, &event, &awakened);
    if (awakened) portYIELD_FROM_ISR();
}

void enqueue_clock(uint8_t source, uint8_t status, uint16_t position = 0)
{
    const ClockEvent event{static_cast<uint64_t>(esp_timer_get_time()), source, status, position};
    xQueueSend(clock_events, &event, 0);
}

const std::array<ModuleParameters, max_patch_modules>& patch_modules()
{
    static const std::array<ModuleParameters, max_patch_modules> modules = {
        voice.oscillator_controls(0), voice.envelope_controls(), voice.filter_controls(),
        voice.oscillator_controls(1), voice.oscillator_controls(2), voice.mixer_controls(),
        voice.lfo_controls(0), voice.lfo_controls(1), voice.lfo_controls(2),
        voice.env_controls(0), voice.env_controls(1), {"MIDI", {}},
        voice.distortion_controls(), voice.delay_controls(),
        voice.sampler_controls(0), voice.sampler_controls(1), voice.sampler_controls(2),
        voice.granulizer_controls(),
        voice.clock_controls(0), voice.clock_controls(1), voice.arp_controls(),
        voice.utility_controls(0), voice.utility_controls(1), voice.utility_controls(2),
        voice.utility_controls(3), voice.utility_controls(4), voice.utility_controls(5),
          voice.noise_controls(), voice.sub_controls(), voice.vca_controls(), voice.ring_controls(),
          voice.wavefolder_controls(),
          voice.pan_controls(0), voice.pan_controls(1), voice.pan_controls(2),
          voice.chorus_controls(), voice.rotary_controls(), {"Stereo", {}}, {"Mono", {}},
          voice.stereo_mix_controls(),
          voice.fm4_controls(), voice.reverb_controls(),
          voice.decimator_controls(), voice.bitcrusher_controls(), voice.compressor_controls(), voice.tb303_controls(),
    };
    return modules;
}

bool stage_patch_waves(const PreparedPatch& patch)
{
    for (unsigned i = 0; i < patch.plan.oscillators; ++i) {
        const int wave = patch.values[oscillator_slots[i]][Osc::wave];
        if (wave >= builtin_waves && !wave_store.read(patch.waves.key(wave).data(), staged_waves[i])) {
            return false;
        }
    }
    return true;
}

using LoadedSamples = std::array<SampleData, sample_slots.size()>;

bool stage_samples(const PreparedPatch& patch, LoadedSamples& data)
{
    for (unsigned i = 0; i < sample_slots.size(); ++i) {
        if (!patch.plan.has(sample_slots[i])) continue;
        bool shared = false;
        for (unsigned other = 0; other < i; ++other) shared |= patch.samples[other] == patch.samples[i];
        if (!shared && !sample_store.load(patch.samples[i].data(), data[i])) return false;
    }
    return true;
}

unsigned sample_owner(const WaveKey& key)
{
    for (unsigned i = 0; i < active_samples.size(); ++i) {
        if (active_sample_keys[i] == key && active_samples[i].pcm) return i;
    }
    return active_samples.size();
}

void update_sample_views()
{
    for (unsigned i = 0; i < active_samples.size(); ++i) {
        const unsigned owner = sample_owner(active_sample_keys[i]);
        if (owner == active_samples.size()) voice.set_sample(i, {});
        else voice.set_sample(i, {active_samples[owner].pcm.get(), active_samples[owner].frames});
    }
}

void install_samples(const PreparedPatch& patch, LoadedSamples&& data)
{
    active_samples = std::move(data);
    active_sample_keys = {};
    active_sample_names = {};
    for (unsigned i = 0; i < sample_slots.size(); ++i) {
        if (!patch.plan.has(sample_slots[i])) continue;
        active_sample_keys[i] = patch.samples[i];
        active_sample_names[i] = patch.sample_names[i];
        if (!sample_store.read_name(active_sample_keys[i].data(), active_sample_names[i]) &&
            !active_sample_names[i][0]) {
            std::strcpy(active_sample_names[i].data(), active_sample_keys[i].data());
        }
    }
    update_sample_views();
}

void install_wave(unsigned oscillator, const WaveKey& key)
{
    const auto& data = staged_waves[oscillator];
    voice.load_wave(oscillator, data);
    loaded_waves[oscillator] = key;
    std::memcpy(wave_names[oscillator], data.name, sizeof(data.name));
    wave_names[oscillator][sizeof(data.name) - 1] = '\0';
}

void apply_values(const PreparedPatch& patch)
{
    active_bank = patch.waves;
    for (unsigned i = 0; i < patch.plan.oscillators; ++i) {
        const int wave = patch.values[oscillator_slots[i]][Osc::wave];
        if (wave >= builtin_waves) {
            install_wave(i, active_bank.key(wave));
        }
    }
    const auto& modules = patch_modules();
    for (unsigned i = 0; i < modules.size(); ++i) {
        if (!patch.plan.has(i)) {
            continue;
        }
        for (unsigned parameter = 0; parameter < modules[i].parameters.size(); ++parameter) {
            modules[i].set(parameter, patch.values[i][parameter]);
        }
    }
    const auto controls = patches.settings();
    for (unsigned i = 0; i < controls.parameters.size(); ++i) {
        controls.set(i, patch.settings[i]);
    }
    active_revision = patch.revision;
    midi_learn = patch.learn;
}

void pause_audio(MidiAction action = MidiAction::pause)
{
    const MidiEvent pause{action, 0, 0, 0, xTaskGetCurrentTaskHandle()};
    xQueueSend(midi_events, &pause, portMAX_DELAY);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

void resume_audio(const AudioPlan* plan = nullptr)
{
    const MidiEvent resume{MidiAction::resume, 0, 0, 0, xTaskGetCurrentTaskHandle(), plan};
    xQueueSend(midi_events, &resume, portMAX_DELAY);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

bool step_sample(unsigned index, int direction)
{
    const std::lock_guard guard(wave_mutex);
    if (recording.busy() || firmware_update_active()) return false;
    static unsigned scanned_revision = ~0u;
    static std::vector<WaveKey> keys;
    const unsigned revision = sample_store.revision();
    if (revision != scanned_revision) {
        keys = sample_store.keys();
        scanned_revision = revision;
    }
    if (keys.size() <= 1) return false;
    const auto found = std::find(keys.begin(), keys.end(), active_sample_keys[index]);
    const int at = found != keys.end() ? found - keys.begin() : 0;
    const int count = keys.size();
    const WaveKey& key = keys[(at + direction + count) % count];
    if (key == active_sample_keys[index]) return false;
    SampleData replacement;
    if (sample_owner(key) == active_samples.size() && !sample_store.load(key.data(), replacement)) return false;
    std::array<char, 32> name{};
    if (!sample_store.read_name(key.data(), name)) {
        const auto other = std::find(active_sample_keys.begin(), active_sample_keys.end(), key);
        if (other != active_sample_keys.end()) name = active_sample_names[other - active_sample_keys.begin()];
        if (!name[0]) std::strcpy(name.data(), key.data());
    }
    pause_audio();
    if (active_samples[index].pcm) {
        for (unsigned other = 0; other < active_samples.size(); ++other) {
            if (other != index && active_sample_keys[other] == active_sample_keys[index]) {
                active_samples[other] = std::move(active_samples[index]);
                break;
            }
        }
    }
    active_samples[index] = std::move(replacement);
    active_sample_keys[index] = key;
    active_sample_names[index] = name;
    update_sample_views();
    resume_audio();
    return true;
}

void play_sample(unsigned index)
{
    const MidiEvent event{MidiAction::play_sample, 0, static_cast<uint8_t>(index), 0};
    xQueueSend(midi_events, &event, 0);
}

bool preview_sample(unsigned index, std::span<char, 32> name, std::span<uint8_t> points, int height)
{
    const std::lock_guard guard(wave_mutex);
    const unsigned owner = sample_owner(active_sample_keys[index]);
    if (owner == active_samples.size()) return false;
    const auto& sample = active_samples[owner];
    const auto& label = active_sample_names[index];
    if (!sample_store.read_name(active_sample_keys[index].data(), name))
        std::strcpy(name.data(), label[0] ? label.data() : active_sample_keys[index].data());
    for (unsigned x = 0; x < points.size(); ++x) {
        const unsigned first = x * sample.frames / points.size();
        const unsigned end = (x + 1) * sample.frames / points.size();
        int peak = sample.pcm[first];
        const unsigned stride = std::max(1u, (end - first) / 32);
        for (unsigned i = first + stride; i < end; i += stride) {
            const int value = sample.pcm[i];
            if (std::abs(value) > std::abs(peak)) peak = value;
        }
        points[x] = ((32767 - peak) * (height - 1)) / 65535;
    }
    return true;
}

const std::vector<WaveKey>& library_keys()
{
    static unsigned revision = ~0u;
    static std::vector<WaveKey> keys;
    if (revision != sample_store.revision()) {
        keys = sample_store.keys();
        revision = sample_store.revision();
    }
    return keys;
}

bool select_library_sample(const WaveKey& key)
{
    SampleData next;
    if (!sample_store.load(key.data(), next)) return false;
    pause_audio(MidiAction::pause_wave);
    library_sample = std::move(next);
    library_key = key;
    if (!sample_store.read_name(key.data(), library_name)) std::strcpy(library_name.data(), key.data());
    voice.set_library_sample({library_sample.pcm.get(), library_sample.frames});
    ++library_revision;
    resume_audio();
    return true;
}

bool step_library_sample(int direction)
{
    const std::lock_guard guard(wave_mutex);
    if (recording.busy()) return false;
    const auto& keys = library_keys();
    if (keys.empty()) return false;
    const auto found = std::find(keys.begin(), keys.end(), library_key);
    const int at = found == keys.end() ? (direction > 0 ? -1 : 0) : found - keys.begin();
    return select_library_sample(keys[(at + direction + int(keys.size())) % keys.size()]);
}

SampleLibraryStatus library_status()
{
    const std::lock_guard guard(wave_mutex);
    static SampleLibraryStatus cached;
    static unsigned storage_revision = ~0u, selection_revision = ~0u;
    const unsigned revision = sample_store.revision();
    if (!recording.busy() && (revision != storage_revision || library_revision != selection_revision)) {
        const auto& keys = library_keys();
        if (!keys.empty() && !library_sample.frames) select_library_sample(keys.front());
        // Selection already loaded its label. Re-read only for an external metadata change.
        if (revision != storage_revision && library_revision == selection_revision) {
            std::array<char, 32> name{};
            if (sample_store.read_name(library_key.data(), name) && name != library_name) {
                library_name = name; ++library_revision;
            }
        }
        const auto found = std::find(keys.begin(), keys.end(), library_key);
        cached.duration_ms = library_sample.frames * 1000 / Recording::sample_rate;
        cached.count = keys.size();
        cached.index = found == keys.end() ? 0u : unsigned(found - keys.begin()) + 1;
        cached.revision = library_revision;
        storage_revision = revision;
        selection_revision = library_revision;
    }
    cached.recording = recording.state();
    cached.milliseconds = recording.frames() * 1000 / Recording::sample_rate;
    return cached;
}

bool library_preview(std::span<char, 32> name, std::span<uint8_t> points, int height)
{
    const std::lock_guard guard(wave_mutex);
    if (!library_sample.frames) return false;
    std::copy(library_name.begin(), library_name.end(), name.begin());
    for (unsigned x = 0; x < points.size(); ++x) {
        const unsigned first = x * library_sample.frames / points.size();
        const unsigned end = (x + 1) * library_sample.frames / points.size();
        int peak = library_sample.pcm[first];
        for (unsigned i = first + 1; i < end; i += std::max(1u, (end - first) / 32))
            if (std::abs(int(library_sample.pcm[i])) > std::abs(peak)) peak = library_sample.pcm[i];
        points[x] = (32767 - peak) * (height - 1) / 65535;
    }
    return true;
}

SampleDeleteStatus delete_sample(const char* id)
{
    const std::lock_guard guard(wave_mutex);
    if (!valid_wave_key(id)) return SampleDeleteStatus::error;
    if (recording.busy() || patch_usb_transfer_active() || sample_store.writing()) return SampleDeleteStatus::busy;
    WaveKey key{}; std::memcpy(key.data(), id, wave_key_size);
    if (std::find(active_sample_keys.begin(), active_sample_keys.end(), key) != active_sample_keys.end())
        return SampleDeleteStatus::in_use;
    bool used = false;
    if (!patches.sample_referenced(key.data(), used)) return SampleDeleteStatus::error;
    if (used) return SampleDeleteStatus::in_use;
    pause_audio(MidiAction::pause_wave);
    const bool removed = sample_store.remove(key.data());
    if (removed && library_key == key) {
        voice.set_library_sample({});
        library_sample = {};
        library_key = {}; library_name = {}; ++library_revision;
    }
    resume_audio();
    return removed ? SampleDeleteStatus::deleted : SampleDeleteStatus::error;
}

SampleDeleteStatus delete_library_sample(unsigned expected_revision)
{
    const std::lock_guard guard(wave_mutex);
    if (expected_revision != library_revision) return SampleDeleteStatus::changed;
    return delete_sample(library_key.data());
}

void play_library_sample()
{
    if (!recording.busy()) play_sample(sample_slots.size());
}

void toggle_recording()
{
    const std::lock_guard guard(wave_mutex);
    const auto state = recording.state();
    if (state == RecordingState::recording || state == RecordingState::armed) { recording.stop(); return; }
    if (recording.busy()) return;
    if (!sample_store.available()) { recording.set(RecordingState::no_storage); return; }
    if (patch_usb_transfer_active() || !sample_store.reserve_recording()) {
        recording.set(RecordingState::storage_busy); return;
    }
    if (recording_data.pcm && recording_data.frames >= 2) {
        pause_audio(); recording.set(RecordingState::ready); resume_audio(); return;
    }
    recording_data.pcm.reset(static_cast<int16_t*>(heap_caps_malloc(
        Recording::max_frames * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!recording_data.pcm) { recording.set(RecordingState::no_memory); return; }
    recording_data.frames = 0;
    recording_prepared = false;
    pause_audio();
    recording.arm({recording_data.pcm.get(), Recording::max_frames});
    resume_audio();
}

void recording_task(void*)
{
    for (;;) {
        if (recording.state() == RecordingState::ready) {
            recording.set(RecordingState::processing);
            if (!recording_prepared) {
                recording_data.frames = Recording::prepare({recording_data.pcm.get(), recording.frames()});
                recording_prepared = true;
            }
            recording.set(RecordingState::saving);
            WaveKey key{}; std::array<char, 32> name{};
            if (sample_store.save_recording({recording_data.pcm.get(), recording_data.frames}, key, name)) {
                const std::lock_guard guard(wave_mutex);
                pause_audio(MidiAction::pause_wave);
                library_sample = std::move(recording_data);
                library_key = key; library_name = name; ++library_revision;
                voice.set_library_sample({library_sample.pcm.get(), library_sample.frames});
                resume_audio();
                recording.set(RecordingState::saved);
            } else recording.set(RecordingState::save_error);
        }
        if (!recording.busy()) {
            const std::lock_guard guard(wave_mutex);
            if (!recording.busy()) sample_store.release_recording();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int maximum_wave()
{
    const std::lock_guard guard(wave_mutex);
    return active_bank.maximum();
}

bool select_wave(unsigned oscillator, int wave)
{
    const std::lock_guard guard(wave_mutex);
    if (recording.busy() || firmware_update_active()) return false;
    const auto module = patch_modules()[oscillator_slots[oscillator]];
    if (wave < 0 || wave > active_bank.maximum()) {
        return false;
    }
    if (wave < builtin_waves || loaded_waves[oscillator] == active_bank.key(wave)) {
        module.set(Osc::wave, wave);
        return true;
    }
    const auto& key = active_bank.key(wave);
    if (!wave_store.read(key.data(), staged_waves[oscillator])) {
        return false;
    }
    pause_audio(MidiAction::pause_wave);
    install_wave(oscillator, key);
    module.set(Osc::wave, wave);
    resume_audio();
    return true;
}

bool preview_wave(unsigned oscillator, int wave, char (&name)[32], std::span<uint8_t> points, int height)
{
    const std::lock_guard guard(wave_mutex);
    if (wave < builtin_waves || wave > active_bank.maximum() ||
        active_bank.key(wave) != loaded_waves[oscillator]) {
        return false;
    }
    std::memcpy(name, wave_names[oscillator], sizeof(name));
    for (unsigned x = 0; x < points.size(); ++x) {
        const auto phase = static_cast<uint32_t>((uint64_t{x} << 32) / (points.size() - 1));
        const float sample = voice.preview_wave(oscillator, phase);
        points[x] = static_cast<uint8_t>((1.0f - sample) * (height - 1) * 0.5f + 0.5f);
    }
    return true;
}

bool prune_waves()
{
    const std::lock_guard guard(wave_mutex);
    std::vector<WaveKey> referenced(active_bank.keys.begin(), active_bank.keys.begin() + active_bank.count);
    return patches.referenced_waves(referenced) && wave_store.prune(referenced);
}

int save_patch(unsigned index)
{
    if (recording.busy() || firmware_update_active()) return PatchStore::storage;
    return patches.save_live(index, active_revision, patch_modules(), midi_learn, active_sample_keys, active_sample_names);
}

ModuleMask load_patch(unsigned index)
{
    const std::lock_guard guard(wave_mutex);
    if (recording.busy() || firmware_update_active()) return 0;
    record_patch_load(index, PatchLoadStage::read);
    const std::unique_ptr<PreparedPatch> patch(new (std::nothrow) PreparedPatch);
    LoadedSamples samples;
    if (!patch || !patches.prepare(index, patch_modules(), *patch)) {
        record_patch_load(index, PatchLoadStage::idle);
        return 0;
    }
    record_patch_load(index, PatchLoadStage::waveform);
    const bool loading_samples = patch->plan.samplers != 0 || patch->plan.has(granulizer);
    if (loading_samples) pause_audio();
    if (!stage_patch_waves(*patch) || !stage_samples(*patch, samples)) {
        if (loading_samples) resume_audio();
        record_patch_load(index, PatchLoadStage::idle);
        return 0;
    }
    record_patch_load(index, PatchLoadStage::effects);
    if (!voice.prepare_patch(patch->plan)) {
        if (loading_samples) resume_audio();
        record_patch_load(index, PatchLoadStage::idle);
        return 0;
    }
    record_patch_load(index, PatchLoadStage::pause);
    if (!loading_samples) pause_audio();
    record_patch_load(index, PatchLoadStage::apply);
    apply_values(*patch);
    install_samples(*patch, std::move(samples));
    record_patch_load(index, PatchLoadStage::resume);
    resume_audio(&patch->plan);
    record_patch_load(index, PatchLoadStage::idle);
    return patch->plan.modules & ~(module_bit(midi) | module_bit(stereo_join) | module_bit(mono_down));
}

bool IRAM_ATTR receive_overflow(i2s_chan_handle_t, i2s_event_data_t*, void*)
{
    input_overflow.store(true, std::memory_order_relaxed);
    return false;
}

void audio_task(void* startup_task)
{
    i2s_chan_handle_t rx = nullptr;
    const i2s_chan_handle_t tx = start_i2s(SynthVoice::sample_rate, block_frames, &rx);
    i2s_event_callbacks_t callbacks{};
    callbacks.on_recv_q_ovf = receive_overflow;
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx, &callbacks, nullptr));
    int16_t stereo[block_frames * 2];
    uint64_t busy_cycles = 0;
    unsigned measured_blocks = 0, active_blocks = 0, peak_cycles = 0, overruns = 0;
    bool paused = false;
    bool patch_loading = false;
    bool updating = false;
    xTaskNotifyGive(static_cast<TaskHandle_t>(startup_task));

    for (;;) {
        if (firmware_update_active() && !updating) {
            voice.all_notes_off(usb, -1, true); voice.all_notes_off(physical, -1, true);
        }
        updating = firmware_update_active();
        const uint32_t start_cycles = esp_cpu_get_cycle_count();
        ClockEvent clock_event{};
        for (unsigned count = 0; count < 64 && xQueueReceive(clock_events, &clock_event, 0) == pdTRUE; ++count) {
            if (!patch_loading && !recording.busy() && !updating) voice.receive_clock(clock_event);
        }
        MidiEvent event{};
        for (unsigned count = 0;
             count < command_queue_capacity && xQueueReceive(midi_events, &event, 0) == pdTRUE;
             ++count) {
            if (event.action == MidiAction::filter_response) {
                voice.publish_filter_response();
                continue;
            }
            if (event.action == MidiAction::pause || event.action == MidiAction::pause_wave) {
                paused = true;
                patch_loading = event.action == MidiAction::pause;
                xTaskNotifyGive(event.reply);
                break;
            }
            if (event.action == MidiAction::resume) {
                if (event.plan) {
                    voice.select_plan(*event.plan);
                } else if (patch_loading) {
                    voice.all_notes_off(usb, -1, true);
                    voice.all_notes_off(physical, -1, true);
                }
                paused = false;
                patch_loading = false;
                xTaskNotifyGive(event.reply);
                continue;
            }
            if (patch_loading || recording.busy() || updating) {
                continue;
            }
            switch (event.action) {
                case MidiAction::play_sample:
                    voice.play_sample(event.note);
                    break;
                case MidiAction::note_on:
                    voice.midi_note_on(event.source, event.channel, event.note, event.velocity);
                    break;
                case MidiAction::note_off:
                    voice.midi_note_off(event.source, event.channel, event.note);
                    break;
                case MidiAction::all_notes_off:
                    voice.midi_clear(event.source, event.channel);
                    break;
                case MidiAction::all_sound_off:
                    voice.midi_clear(event.source, event.channel, true);
                    break;
                case MidiAction::disconnect:
                    voice.midi_clear(event.source);
                    break;
                case MidiAction::pause:
                case MidiAction::pause_wave:
                case MidiAction::resume:
                case MidiAction::filter_response:
                    break;
            }
        }

        if (!paused && recording.state() == RecordingState::armed) {
            voice.midi_clear(usb, -1, true);
            voice.midi_clear(physical, -1, true);
            voice.stop_sample_preview();
            input_overflow.store(false, std::memory_order_relaxed);
            if (recording.stopped()) recording.set(RecordingState::idle);
            else recording.set(i2s_channel_enable(rx) == ESP_OK ? RecordingState::recording : RecordingState::input_error);
        }
        if (!paused && recording.state() == RecordingState::recording) {
            bool complete = recording.stopped();
            bool failed = false;
            if (!complete) {
                size_t received = 0;
                const esp_err_t result = i2s_channel_read(rx, stereo, sizeof(stereo), &received, 100);
                failed = result != ESP_OK || received == 0 || received % (2 * sizeof(int16_t)) ||
                    input_overflow.load(std::memory_order_relaxed);
                if (!failed) complete = recording.capture({stereo, received / sizeof(int16_t)});
            }
            if (complete || failed) {
                const bool disabled = i2s_channel_disable(rx) == ESP_OK;
                recording.set(failed || !disabled ? RecordingState::input_error :
                    recording.frames() >= 2 ? RecordingState::ready : RecordingState::idle);
            }
        }
        if (paused || recording.busy() || updating) {
            std::fill_n(stereo, 2 * block_frames, int16_t{0});
        } else {
            voice.set_time(esp_timer_get_time());
            voice.render(stereo);
        }
        if (!paused && !recording.busy() && !updating) {
            const unsigned elapsed = uint32_t(esp_cpu_get_cycle_count() - start_cycles);
            busy_cycles += elapsed; peak_cycles = std::max(peak_cycles, elapsed); ++active_blocks;
            constexpr unsigned deadline = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1'000'000ULL * block_frames / SynthVoice::sample_rate;
            overruns += elapsed > deadline;
        }
        if (++measured_blocks == load_window_blocks) {
            const auto load = (busy_cycles + cycles_per_tenth_percent / 2) /
                              cycles_per_tenth_percent;
            audio_load_tenths.store(std::min(load, uint64_t{9999}), std::memory_order_relaxed);
            audio_peak_tenths.store(std::min(uint64_t{9999}, uint64_t{peak_cycles} * 1000 * SynthVoice::sample_rate /
                (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1'000'000ULL * block_frames)), std::memory_order_relaxed);
            audio_overruns.store(overruns, std::memory_order_relaxed);
            audio_load_valid.store(active_blocks == measured_blocks, std::memory_order_relaxed);
            busy_cycles = 0; peak_cycles = overruns = active_blocks = 0;
            measured_blocks = 0;
        }

        size_t written = 0;
        const esp_err_t result = i2s_channel_write(tx, stereo, sizeof(stereo), &written, 100);
        if (result != ESP_OK) {
            record_driver_error(DriverOperation::audio_write, result);
            ESP_ERROR_CHECK(result);
        }
        if (result == ESP_OK && written == sizeof(stereo)) firmware_boot_audio_ok();
    }
}

void enqueue_midi(uint8_t status_byte, uint8_t data1, uint8_t data2, MidiSource source)
{
    const uint8_t status = status_byte >> 4;
    const uint8_t channel = status_byte & 0x0f;
    MidiAction action;
    if (status == 0x09 && data2 > 0) {
        action = MidiAction::note_on;
    } else if (status == 0x08 || status == 0x09) {
        action = MidiAction::note_off;
    } else if (status == 0x0b && data1 == 123) {
        action = MidiAction::all_notes_off;
    } else if (status == 0x0b && data1 == 120) {
        action = MidiAction::all_sound_off;
    } else if (status == 0x0b && data1 < 120) {
        const int selected = settings.parameters[midi_channel].get();
        if (!selected || channel + 1 == selected) post_midi_cc(data1, data2);
        return;
    } else {
        return;
    }

    // A full queue can wait on the MIDI core. The audio core never waits for MIDI.
    const MidiEvent event{action, channel, data1, data2, nullptr, nullptr, source};
    xQueueSend(midi_events, &event, portMAX_DELAY);
}

struct SerialMidi {
    uint8_t status = 0;
    uint8_t first = 0;
    bool second = false;
    uint8_t song_position = 0;

    void receive(uint8_t byte)
    {
        if (byte >= 0xf8) {
            if (byte == 0xf8 || byte == 0xfa || byte == 0xfb || byte == 0xfc) enqueue_clock(2, byte);
            return;  // Real-time MIDI does not interrupt running status.
        }
        if (byte & 0x80) {
            status = byte < 0xf0 || byte == 0xf2 ? byte : 0;
            second = false;
        } else if (status) {
            const unsigned type = status >> 4;
            if (status == 0xf2) {
                if (!second) song_position = byte;
                else {
                    enqueue_clock(2, 0xf2, song_position | (uint16_t{byte} << 7));
                    status = 0;
                }
                second = !second;
                return;
            }
            if (type == 0xc || type == 0xd) {
                return;  // One data byte, unused by the synth.
            }
            if (second) {
                enqueue_midi(status, first, byte, physical);
            } else {
                first = byte;
            }
            second = !second;
        }
    }
};

#ifdef MODULOX_USB_DIAGNOSTICS
void usb_trace(const char* stage)
{
    char record[480];
    std::snprintf(record, sizeof(record), "USBTRACE:%s t=%lld c=%d m=%d rtc=%08lx usj=%08lx wrap=%08lx ahb=%08lx usb=%08lx rst=%08lx gotg=%08lx cfg=%08lx ctl=%08lx sts=%08lx irq=%08lx mask=%08lx END",
        stage, (long long)esp_timer_get_time(), tud_connected(), tud_mounted(),
        (unsigned long)REG_READ(RTC_CNTL_USB_CONF_REG), (unsigned long)REG_READ(USB_SERIAL_JTAG_CONF0_REG),
        (unsigned long)REG_READ(USB_WRAP_OTG_CONF_REG), (unsigned long)REG_READ(USB_GAHBCFG_REG),
        (unsigned long)REG_READ(USB_GUSBCFG_REG), (unsigned long)REG_READ(USB_GRSTCTL_REG),
        (unsigned long)REG_READ(USB_GOTGCTL_REG), (unsigned long)REG_READ(USB_DCFG_REG),
        (unsigned long)REG_READ(USB_DCTL_REG), (unsigned long)REG_READ(USB_DSTS_REG),
        (unsigned long)REG_READ(USB_GINTSTS_REG), (unsigned long)REG_READ(USB_GINTMSK_REG));
    nvs_handle_t handle;
    if (nvs_flash_init() == ESP_OK && nvs_open("usb_diag", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, stage, record); nvs_commit(handle); nvs_close(handle);
    }
}
#endif

void usb_midi_task(void*)
{
#ifdef MODULOX_USB_DIAGNOSTICS
    usb_trace("before");
#endif
    // ROM exposes Serial/JTAG on the same USB pins. A PHY switch alone may
    // leave Windows attached to that old device. Give the host a real detach
    // interval before TinyUSB takes ownership and advertises MIDI + CDC.
    usb_serial_jtag_ll_phy_enable_pad(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    tinyusb_config_t config{};
    config.string_descriptor = usb_strings;
    config.string_descriptor_count = sizeof(usb_strings) / sizeof(usb_strings[0]);
    config.configuration_descriptor = usb_config;
    ESP_ERROR_CHECK(tinyusb_driver_install(&config));
    // Disconnect through the OTG controller after the PHY handoff, so both
    // USB data lines are actually pulled low long enough for the host.
    tud_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));
    tud_connect();
    // Match Arduino's explicit USB_PHY_SPEED_FULL setup: advertise on D+.
    // The DWC2 connect routine clears the PHY pull override during init.
    usb_wrap_pull_override_vals_t usb_pulls{};
    usb_pulls.dp_pu = true;
    usb_wrap_ll_phy_enable_pull_override(&USB_WRAP, &usb_pulls);
#ifdef MODULOX_USB_DIAGNOSTICS
    usb_trace("ready");
    bool trace_recorded = false;
#endif
    firmware_boot_ready(firmware_usb_ready);
    ESP_LOGI(tag, "Modulox Sine USB MIDI ready");

    uart_config_t uart{};
    uart.baud_rate = 31250;
    uart.data_bits = UART_DATA_8_BITS;
    uart.parity = UART_PARITY_DISABLE;
    uart.stop_bits = UART_STOP_BITS_1;
    uart.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart.source_clk = UART_SCLK_DEFAULT;
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, GPIO_NUM_21,
                                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 256, 0, 0, nullptr, 0));
    SerialMidi din;
    bool was_mounted = false;
    for (;;) {
#ifdef MODULOX_USB_DIAGNOSTICS
        if (!trace_recorded && esp_timer_get_time() > 12000000) { usb_trace("later"); trace_recorded = true; }
#endif
        const bool mounted = tud_mounted();
        if (was_mounted && !mounted) {
            const MidiEvent event{MidiAction::disconnect, 0, 0, 0};
            xQueueSend(midi_events, &event, portMAX_DELAY);
        }
        was_mounted = mounted;

        uint8_t packet[4];
        while (mounted && tud_midi_packet_read(packet)) {
            const auto cin = packet[0] & 0xf;
            if (cin == 0xf && (packet[1] == 0xf8 || packet[1] == 0xfa || packet[1] == 0xfb || packet[1] == 0xfc)) {
                enqueue_clock(1, packet[1]);
            } else if (cin == 3 && packet[1] == 0xf2 && packet[2] < 128 && packet[3] < 128) {
                enqueue_clock(1, 0xf2, packet[2] | (uint16_t{packet[3]} << 7));
            } else if (cin == (packet[1] >> 4) && packet[2] < 128 && packet[3] < 128) {
                enqueue_midi(packet[1], packet[2], packet[3], usb);
            }
        }
        uint8_t bytes[32];
        const int count = uart_read_bytes(UART_NUM_2, bytes, sizeof(bytes), 0);
        for (int i = 0; i < count; ++i) {
            din.receive(bytes[i]);
        }
        {
            const std::lock_guard guard(wave_mutex);
            service_patch_usb(patches, patch_modules(), wave_store, sample_store, prune_waves, recording.busy(), delete_sample);
        }
        vTaskDelay(1);
    }
}

}  // namespace

bool resource_info(cJSON* object)
{
    const std::lock_guard guard(wave_mutex);
    constexpr auto external = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    constexpr auto internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    auto* allocated = cJSON_AddObjectToObject(object, "allocatedEffects");
    if (!allocated) return false;
    for (const int slot : {delay, chorus, rotary, reverb})
        if (!cJSON_AddNumberToObject(allocated, patch_module_types[slot], voice.effect_allocated_bytes(slot))) return false;
    return cJSON_AddNumberToObject(object, "version", 1) &&
        cJSON_AddNumberToObject(object, "timestampMs", esp_timer_get_time() / 1000) &&
        cJSON_AddNumberToObject(object, "psramTotal", heap_caps_get_total_size(external)) &&
        cJSON_AddNumberToObject(object, "psramFree", heap_caps_get_free_size(external)) &&
        cJSON_AddNumberToObject(object, "psramLargest", heap_caps_get_largest_free_block(external)) &&
        cJSON_AddNumberToObject(object, "internalFree", heap_caps_get_free_size(internal)) &&
        cJSON_AddNumberToObject(object, "internalLargest", heap_caps_get_largest_free_block(internal)) &&
        cJSON_AddNumberToObject(object, "cpuAverage", audio_load_tenths.load(std::memory_order_relaxed) * 0.1) &&
        cJSON_AddNumberToObject(object, "cpuPeak", audio_peak_tenths.load(std::memory_order_relaxed) * 0.1) &&
        cJSON_AddNumberToObject(object, "overBudgetBlocks", audio_overruns.load(std::memory_order_relaxed)) &&
        cJSON_AddBoolToObject(object, "cpuValid", audio_load_valid.load(std::memory_order_relaxed) && !recording.busy() && !firmware_update_active());
}

extern "C" void app_main()
{
    firmware_boot_begin();
    start_runtime_diagnostics();
    voice.prepare();
    wave_store.mount();
    sample_store.mount();
    patches.mount(patch_modules(), settings.controls(), wave_store, sample_store);
    if (sample_store.available() && wave_store.available() && patches.available()) firmware_boot_ready(firmware_storage_ready);
    int initial_patch = -1;
    ModuleMask module_mask = default_audio_plan().modules;
    {
        const std::unique_ptr<PreparedPatch> initial(new (std::nothrow) PreparedPatch);
        LoadedSamples samples;
        if (initial && patches.prepare(0, patch_modules(), *initial) && stage_patch_waves(*initial) &&
            stage_samples(*initial, samples) && voice.prepare_patch(initial->plan)) {
            apply_values(*initial);
            install_samples(*initial, std::move(samples));
            voice.select_plan(initial->plan);
            initial_patch = 0;
            module_mask = initial->plan.modules & ~(module_bit(midi) | module_bit(stereo_join) | module_bit(mono_down));
        }
    }
    midi_events = xQueueCreate(command_queue_capacity, sizeof(MidiEvent));
    clock_events = xQueueCreate(64, sizeof(ClockEvent));
    ESP_ERROR_CHECK(midi_events && clock_events ? ESP_OK : ESP_ERR_NO_MEM);
    gpio_config_t sync{};
    sync.pin_bit_mask = 1ULL << GPIO_NUM_48;
    sync.mode = GPIO_MODE_INPUT;
    sync.pull_down_en = GPIO_PULLDOWN_ENABLE;
    sync.intr_type = GPIO_INTR_POSEDGE;
    ESP_ERROR_CHECK(gpio_config(&sync));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_48, sync_edge, nullptr));
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(audio_task, "audio", 4096, xTaskGetCurrentTaskHandle(),
                                          20, nullptr, 1)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    start_headphone_codec();
    firmware_boot_ready(firmware_codec_ready);
    set_headphone_volume(settings.parameters[master_volume].get());
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(recording_task, "record_save", 6144, nullptr, 2, nullptr, 0)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    start_controls(patch_modules(), audio_load_tenths, patches, load_patch, save_patch,
                   {maximum_wave, select_wave, preview_wave}, {step_sample, play_sample, preview_sample},
                   {step_library_sample, play_library_sample, toggle_recording, library_status, library_preview,
                    [] { return recording.state(); }, delete_library_sample},
                   {[] { return voice.base_filter_response(); },
                    [](Filter::Response& response) { return voice.read_filter_response(response); },
                    [] {
                        if (!voice.request_filter_response()) return;
                        const MidiEvent event{MidiAction::filter_response, 0, 0, 0};
                        if (xQueueSend(midi_events, &event, 0) != pdTRUE) voice.cancel_filter_response();
                    }}, midi_learn,
                   initial_patch, module_mask);
    firmware_boot_ready(firmware_controls_ready);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(usb_midi_task, "usb_midi", 6144, nullptr, 6, nullptr, 0)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

