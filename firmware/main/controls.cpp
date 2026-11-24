#include "controls.h"
#include "audio_output.h"
#include "display.h"
#include "filter_plot.h"
#include "lfo.h"
#include "module_menu.h"
#include "osc.h"
#include "sampler.h"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <utility>

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace {

constexpr gpio_num_t encoder_a = GPIO_NUM_6;
constexpr gpio_num_t encoder_b = GPIO_NUM_7;
constexpr gpio_num_t menu_pin = GPIO_NUM_0;
using enum PanelInput;
constexpr std::array buttons{
    std::pair{GPIO_NUM_12, left}, std::pair{GPIO_NUM_1, center}, std::pair{GPIO_NUM_38, right},
    std::pair{menu_pin, menu}, std::pair{GPIO_NUM_39, encoder_press},
};
enum class Page { parameters, modules, categories, patches, samples };
enum class SaveStatus { none, saved, changed, error };
constexpr int settings_module = -1;
constexpr int save_action = patch_setting_count;

constexpr int visible_count = 4; // Parameter buttons; menus use five independent rows.
static_assert(menu_top + menu_visible_rows * menu_row_height == display::height);
constexpr uint16_t black = 0x0000;
constexpr uint16_t white = 0xffff;
constexpr uint16_t tile = 0x39c7;
constexpr uint16_t accent = 0x3fff;
constexpr int load_height = 12;
constexpr int load_width = 38;
constexpr int load_margin = 10;
constexpr TickType_t load_refresh_interval = pdMS_TO_TICKS(1000);
constexpr TickType_t filter_refresh_interval = pdMS_TO_TICKS(100);
TickType_t last_filter_refresh = 0;

struct ButtonState {
    bool reading = false;
    bool pressed = false;
    TickType_t changed_at = 0;

    bool poll(gpio_num_t pin, TickType_t now)
    {
        const bool down = gpio_get_level(pin) == 0;
        if (down != reading) {
            reading = down;
            changed_at = now;
        }
        if (reading != pressed && now - changed_at >= pdMS_TO_TICKS(15)) {
            pressed = reading;
            return pressed;
        }
        return false;
    }
};

struct Encoder {
    unsigned previous = 3;
    int edges = 0;

    int poll()
    {
        // A is the low bit, matching Rotary(6, 7) in the Arduino firmware.
        constexpr int transitions[4][4] = {
            {0, -1, 1, 0}, {1, 0, 0, -1}, {-1, 0, 0, 1}, {0, 1, -1, 0},
        };
        const unsigned current = gpio_get_level(encoder_a) | (gpio_get_level(encoder_b) << 1);
        if (current == previous) {
            return 0;
        }
        const int delta = transitions[previous][current];
        previous = current;
        edges = delta == 0 ? 0 : edges + delta;
        if (current != 3) {
            return 0;
        }
        const int step = edges == 4 ? 1 : edges == -4 ? -1 : 0;
        edges = 0;
        return step;
    }
};

struct Selection {
    int module = 0;
    int parameter = 0;
    int menu_module = 0;
    int menu_patch = 0;
    int menu_category = 0;
    PatchCategory category = PatchCategory::user;
    int active_patch = -1;
    ModuleMask module_mask = 0;
    Page page = Page::parameters;
    bool patch_error = false;
    bool learning = false;
    int learned_cc = -1;
    unsigned sample_revision = 0;
    bool dirty = false;
    SaveStatus save_status = SaveStatus::none;
    bool confirm_sample_delete = false;
    unsigned delete_sample_revision = 0;
    SampleDeleteStatus sample_delete_status = SampleDeleteStatus::none;
};

std::span<const ModuleParameters> modules;
const PatchStore* patches;
ModuleMask (*load_patch)(unsigned);
int (*save_patch)(unsigned);
WaveControls wave_controls;
SampleControls sample_controls;
SampleLibraryControls sample_library;
FilterDisplayControls filter_display;
MidiLearnMap* midi_learn;
struct ControlChange { uint8_t number; uint8_t value; };
QueueHandle_t control_changes;
Selection initial_selection;
const std::atomic<unsigned>* audio_load_tenths;
QueueHandle_t selection_updates;
QueueHandle_t remote_controls;

void draw_load(unsigned load)
{
    char label[7];
    load = std::min(load, 9999u);
    std::snprintf(label, sizeof(label), "%u.%u%%", load / 10, load % 10);
    display::text(display::width - load_width - load_margin, 20, load_width, load_height,
                  label, 1, white, tile);
}

void draw_title(std::string_view title)
{
    display::heading(title);
    draw_load(audio_load_tenths->load(std::memory_order_relaxed));
}

unsigned module_slot(ModuleMask mask, int index)
{
    return menu_module_slot(mask, index);
}

ModuleMask editable_modules(ModuleMask mask)
{
    for (unsigned slot = 0; slot < modules.size(); ++slot)
        if (modules[slot].parameters.empty()) mask &= ~module_bit(slot);
    return mask;
}

const ModuleParameters& module_at(ModuleMask mask, int index)
{
    return modules[module_slot(mask, index)];
}

bool saving_patch(const Selection& selection)
{
    return selection.module == settings_module && selection.parameter == save_action;
}

void save_active_patch(Selection& selection)
{
    if (selection.active_patch < 0) {
        return;
    }
    const int result = save_patch(selection.active_patch);
    selection.save_status = result >= 0 ? SaveStatus::saved
        : result == PatchStore::changed ? SaveStatus::changed : SaveStatus::error;
    if (result >= 0) {
        selection.dirty = false;
    }
}

ModuleParameters selected_module(const Selection& selection)
{
    return selection.module == settings_module ? patches->settings()
                                                : module_at(selection.module_mask, selection.module);
}

unsigned selected_slot(const Selection& selection)
{
    return selection.module == settings_module ? max_patch_modules
        : module_slot(selection.module_mask, selection.module);
}

bool is_sampler_slot(unsigned slot)
{
    return slot >= sampler && slot <= sampler3;
}

constexpr int sample_actions = 2;

int parameter_offset(unsigned slot)
{
    return sample_slot(slot) ? sample_actions : 0;
}

bool on_sample_page(const Selection& selection)
{
    return selection.page == Page::parameters && sample_slot(selected_slot(selection)) &&
           selection.parameter < (is_sampler_slot(selected_slot(selection)) ? 4 : sample_actions);
}

bool on_sample_action(const Selection& selection)
{
    return on_sample_page(selection) && selection.parameter < sample_actions;
}

int parameter_count(const Selection& selection)
{
    return selected_module(selection).visible_count() + parameter_offset(selected_slot(selection));
}

unsigned parameter_index(const Selection& selection)
{
    return selected_module(selection).parameter_at(selection.parameter - parameter_offset(selected_slot(selection)));
}

void publish(Selection& selection)
{
    selection.learned_cc = -1;
    if (selection.page == Page::parameters && !saving_patch(selection) && !on_sample_action(selection)) {
        const auto slot = selected_slot(selection);
        const unsigned index = parameter_index(selection);
        if (slot < midi_learn->cc.size() && index < midi_learn->cc[slot].size()) {
            selection.learned_cc = midi_learn->cc[slot][index];
        }
    }
    xQueueOverwrite(selection_updates, &selection);
}

void set_parameter(Selection& selection, int value)
{
    const auto module = selected_module(selection);
    const int index = parameter_index(selection);
    const int before = module.parameters[index].get();
    const unsigned slot = selected_slot(selection);
    if (is_sampler_slot(slot)) {
        if (index == Sampler::start) value = std::min(value, module.parameters[Sampler::end].get() - 1);
        if (index == Sampler::end) value = std::max(value, module.parameters[Sampler::start].get() + 1);
    }
    const auto oscillator = std::find(oscillator_slots.begin(), oscillator_slots.end(), slot);
    if (index == Osc::wave && oscillator != oscillator_slots.end()) {
        const int wave = std::clamp(value, 0, wave_controls.maximum());
        if (wave != before && !wave_controls.select(oscillator - oscillator_slots.begin(), wave)) {
            return;
        }
    } else {
        module.set(index, value);
    }
    const int after = module.parameters[index].get();
    if (before != after) {
        selection.dirty = true;
        if (selection.module == settings_module && index == master_volume) {
            set_headphone_volume(after);
        }
    }
}

void apply_cc(Selection& selection, ControlChange cc)
{
    if (selection.learning) {
        const auto slot = selected_slot(selection);
        const unsigned index = parameter_index(selection);
        selection.learning = false;
        if (slot < midi_learn->cc.size() && index < midi_learn->cc[slot].size()) {
            midi_learn->cc[slot][index] = cc.number;
            selection.dirty = true;
        }
    }
    Selection target = selection;
    const int count = std::popcount(selection.module_mask);
    for (int module = settings_module; module < count; ++module) {
        target.module = module;
        const auto slot = selected_slot(target);
        if (slot >= midi_learn->cc.size()) continue;
        const auto controls = selected_module(target);
        const auto parameters = controls.parameters;
        for (unsigned i = 0; i < std::min(parameters.size(), midi_learn->cc[slot].size()); ++i) {
            if (!controls.visible(i)) continue;
            if (midi_learn->cc[slot][i] != cc.number) continue;
            target.parameter = controls.parameter_position(i) + parameter_offset(slot);
            const auto& parameter = parameters[i];
            const bool wave = std::string_view(parameter.name) == "Wave" &&
                              parameter.maximum >= builtin_waves;
            const int maximum = wave ? wave_controls.maximum() : parameter.maximum;
            set_parameter(target, parameter.minimum + ((maximum - parameter.minimum) * cc.value + 63) / 127);
        }
    }
    selection.dirty = target.dirty;
}

void move_selection(Selection& selection, int direction)
{
    const int count = std::popcount(selection.module_mask);
    if (selection.page == Page::samples) {
        selection.confirm_sample_delete = false;
        selection.sample_delete_status = SampleDeleteStatus::none;
        selection.parameter = (selection.parameter + direction + 4) % 4;
        return;
    }
    if (selection.page == Page::categories) {
        const int count = category_menu_count(patches->category_counts());
        selection.menu_category = (selection.menu_category + direction + count) % count;
        return;
    }
    if (selection.page == Page::patches) {
        const int count = patches->category_counts()[static_cast<unsigned>(selection.category)];
        if (count > 0) {
            selection.menu_patch = (selection.menu_patch + direction + count) % count;
        }
        selection.patch_error = false;
        return;
    }
    if (selection.page == Page::modules) {
        selection.menu_module = (selection.menu_module + direction + count + 2) % (count + 2);
        return;
    }
    if (selection.module == settings_module) {
        constexpr int count = patch_setting_count + 1;
        selection.parameter = (selection.parameter + direction + count) % count;
        return;
    }
    selection.parameter += direction;
    if (selection.parameter >= parameter_count(selection)) {
        selection.module = (selection.module + 1) % count;
        selection.parameter = 0;
    } else if (selection.parameter < 0) {
        selection.module = (selection.module + count - 1) % count;
        selection.parameter = parameter_count(selection) - 1;
    }
}

void apply_button(Selection& selection, PanelInput input)
{
    const bool was_learning = selection.learning;
    selection.learning = false;
    selection.save_status = SaveStatus::none;
    if (input != center) {
        selection.confirm_sample_delete = false;
        selection.sample_delete_status = SampleDeleteStatus::none;
    }
    switch (input) {
        case left:
            move_selection(selection, -1);
            break;
        case right:
            move_selection(selection, 1);
            break;
        case center:
            if (selection.page == Page::categories) {
                const auto counts = patches->category_counts();
                selection.menu_category = std::clamp(selection.menu_category, 0, int(category_menu_count(counts)) - 1);
                selection.category = category_menu_at(counts, selection.menu_category);
                selection.menu_patch = 0;
                selection.patch_error = false;
                selection.page = Page::patches;
            } else if (selection.page == Page::patches) {
                const int count = patches->category_counts()[static_cast<unsigned>(selection.category)];
                selection.menu_patch = std::clamp(selection.menu_patch, 0, std::max(0, count - 1));
                const int index = patches->category_patch(selection.category, selection.menu_patch);
                if (index >= 0) {
                    const ModuleMask mask = load_patch(index);
                    if (mask) {
                        selection.active_patch = index;
                        selection.module_mask = editable_modules(mask);
                        selection.module = selection.module_mask ? 0 : settings_module;
                        selection.parameter = 0;
                        selection.page = Page::parameters;
                        selection.dirty = false;
                        xQueueReset(control_changes);
                        set_headphone_volume(patches->settings().parameters[master_volume].get());
                    } else {
                        selection.patch_error = true;
                    }
                }
            } else if (selection.page == Page::samples) {
                if (selection.parameter == 1) sample_library.play();
                if (selection.parameter == 2) sample_library.record();
                if (selection.parameter == 3) {
                    if (selection.confirm_sample_delete) {
                        selection.sample_delete_status = sample_library.remove(selection.delete_sample_revision);
                        selection.confirm_sample_delete = false;
                    } else {
                        const auto status = sample_library.status();
                        if (status.index) {
                            selection.delete_sample_revision = status.revision;
                            selection.confirm_sample_delete = true;
                            selection.sample_delete_status = SampleDeleteStatus::none;
                        }
                    }
                }
            } else if (selection.page == Page::modules) {
                if (selection.menu_module == std::popcount(selection.module_mask) + 1) {
                    selection.parameter = 0;
                    selection.page = Page::samples;
                    break;
                }
                selection.module = selection.menu_module == std::popcount(selection.module_mask)
                    ? settings_module : selection.menu_module;
                selection.parameter = 0;
                selection.page = Page::parameters;
            } else if (saving_patch(selection)) {
                save_active_patch(selection);
            } else if (on_sample_action(selection)) {
                if (selection.parameter == 1) sample_controls.play(selected_slot(selection) - sampler);
            } else {
                selection.learning = !was_learning;
            }
            break;
        case menu:
            selection.menu_module = selection.page == Page::samples
                ? std::popcount(selection.module_mask) + 1 : selection.module == settings_module
                    ? std::popcount(selection.module_mask) : selection.module;
            selection.page = selection.page == Page::modules
                ? Page::parameters : Page::modules;
            break;
        case encoder_press:
            if (selection.page == Page::patches) {
                selection.page = Page::categories;
            } else if (selection.page == Page::categories) {
                selection.page = Page::parameters;
            } else {
                selection.page = Page::categories;
                selection.menu_category = 0;
            }
            selection.patch_error = false;
            break;
        case clockwise:
        case counterclockwise:
            return;
    }
    publish(selection);
}

void apply_encoder(Selection& selection, int step)
{
    if (step != 0 && (selection.page != Page::parameters || !saving_patch(selection))) {
        selection.learning = false;
        selection.save_status = SaveStatus::none;
        if (selection.page == Page::samples) {
            if (selection.parameter == 0) {
                selection.sample_delete_status = SampleDeleteStatus::none;
                sample_library.step(step);
            }
        } else if (selection.page != Page::parameters) {
            move_selection(selection, step);
        } else if (on_sample_action(selection)) {
            if (selection.parameter == 0 && sample_controls.step(selected_slot(selection) - sampler, step)) {
                ++selection.sample_revision;
                selection.dirty = true;
            }
        } else {
            const auto module = selected_module(selection);
            const auto& parameter = module.parameters[parameter_index(selection)];
            const int value = parameter.get();
            const int increment = parameter.step_divisor == 0
                ? parameter.step : std::max(parameter.step, value / parameter.step_divisor);
            set_parameter(selection, value + step * increment);
        }
        publish(selection);
    }
}

void input_task(void*)
{
    gpio_config_t config{};
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pin_bit_mask = (1ULL << encoder_a) | (1ULL << encoder_b) | (1ULL << menu_pin);
    ESP_ERROR_CHECK(gpio_config(&config));
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.pin_bit_mask = 0;
    for (const auto& [pin, input] : buttons) {
        if (input != menu) config.pin_bit_mask |= 1ULL << pin;
    }
    ESP_ERROR_CHECK(gpio_config(&config));

    std::array<ButtonState, buttons.size()> button_state{};
    Encoder encoder;
    Selection selection = initial_selection;
    publish(selection);
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        for (unsigned i = 0; i < buttons.size(); ++i) {
            const auto [pin, input] = buttons[i];
            if (!button_state[i].poll(pin, now)) {
                continue;
            }
            apply_button(selection, input);
        }
        apply_encoder(selection, encoder.poll());
        PanelInput input;
        for (int count = 0; count < 8 && xQueueReceive(remote_controls, &input, 0) == pdTRUE; ++count) {
            if (input == clockwise || input == counterclockwise) {
                apply_encoder(selection, input == clockwise ? 1 : -1);
            } else {
                apply_button(selection, input);
            }
        }
        ControlChange cc;
        for (int count = 0; count < 8 && xQueueReceive(control_changes, &cc, 0) == pdTRUE; ++count) {
            apply_cc(selection, cc);
            publish(selection);
        }
        vTaskDelay(1);
    }
}

std::string_view format_value(const Parameter& parameter, int value, char (&buffer)[24])
{
    if (value >= parameter.minimum && unsigned(value - parameter.minimum) < parameter.choices.size()) {
        return parameter.choices[value - parameter.minimum];
    }
    std::snprintf(buffer, sizeof(buffer), "%d%s%s", value,
                  parameter.unit[0] ? " " : "", parameter.unit);
    return buffer;
}

constexpr int plot_height = 88;

template<class Sample>
void draw_wave(uint64_t periods, Sample sample)
{
    constexpr float half_height = (plot_height - 1) * 0.5f;
    std::array<uint8_t, display::width> points;
    for (unsigned x = 0; x < points.size(); ++x) {
        const auto phase = static_cast<uint32_t>(periods * x / (points.size() - 1));
        points[x] = static_cast<uint8_t>((1.0f - sample(phase)) * half_height + 0.5f);
    }
    display::waveform(90, plot_height, points);
}

std::string_view draw_osc_wave(const ModuleParameters& module, unsigned oscillator, bool entering)
{
    const int wave = module.parameters[Osc::wave].get();
    const int pwm = module.parameters[Osc::pwm].get();
    static int previous_wave = -1;
    static int previous_pwm = -1;
    static char name[32];
    if (!entering && wave == previous_wave && pwm == previous_pwm) {
        return name;
    }
    if (wave >= builtin_waves) {
        std::array<uint8_t, display::width> points;
        if (!wave_controls.preview(oscillator, wave, name, points, plot_height)) {
            return {};
        }
        display::waveform(90, plot_height, points);
    } else {
        name[0] = '\0';
        draw_wave(1ull << 32, [=](uint32_t phase) {
            return Osc::preview_sample(wave, phase, pwm);
        });
    }
    previous_wave = wave;
    previous_pwm = pwm;
    return name;
}

void draw_lfo_wave(const ModuleParameters& module, bool entering)
{
    const auto& parameters = module.parameters;
    const LfoSettings settings{parameters[Lfo::wave].get(), parameters[Lfo::rate].get(),
                               parameters[Lfo::depth].get(), parameters[Lfo::level].get()};
    static LfoSettings previous{};
    if (!entering && settings == previous) {
        return;
    }
    previous = settings;
    // Expand the preview from one to eight periods independently of its timebase.
    const uint64_t periods = (1ull << 32) +
        (7ull << 32) * (settings.rate - 1) / (Lfo::max_rate - 1);
    Modulation modulation{0.0f, settings.depth * 0.01f, settings.level * 0.01f};
    draw_wave(periods, [&](uint32_t phase) {
        modulation.position = 0.5f + 0.5f * Lfo::waveform(settings.wave, phase);
        return modulation.apply(0, -1, 1, 1);
    });
}

void draw_menu(const Selection& selection)
{
    const bool patch_menu = selection.page == Page::patches;
    const bool category_menu = selection.page == Page::categories;
    const auto counts = patches->category_counts();
    const int items = category_menu ? category_menu_count(counts)
        : patch_menu ? counts[static_cast<unsigned>(selection.category)]
                     : std::popcount(selection.module_mask);
    const int count = items + (patch_menu || category_menu ? 0 : 2);
    const int requested = category_menu ? selection.menu_category : patch_menu ? selection.menu_patch : selection.menu_module;
    const int cursor = count ? std::clamp(requested, 0, count - 1) : 0;
    const char* title = category_menu ? "Categories" : patch_menu
        ? patch_categories[static_cast<unsigned>(selection.category)].label : "Modules";
    if (patch_menu && items == 0) {
        title = "Empty";
    } else if (patch_menu && selection.patch_error) {
        title = "Invalid patch";
    }
    draw_title(title);
    const int first = cursor / menu_visible_rows * menu_visible_rows;
    for (unsigned i = 0; i < menu_visible_rows; ++i) {
        const int index = first + i;
        const bool selected = index < count && index == cursor;
        PatchEntry patch{};
        char category_label[32]{};
        std::string_view label;
        if (index < count) {
            if (category_menu) {
                const auto category = static_cast<unsigned>(category_menu_at(counts, index));
                std::snprintf(category_label, sizeof(category_label), "%s (%u)", patch_categories[category].label, counts[category]);
                label = category_label;
            } else if (!patch_menu && index >= items) {
                label = index == items ? "Settings" : "Samples";
            } else if (patch_menu) {
                const int patch_index = patches->category_patch(selection.category, index);
                if (patch_index >= 0 && patches->entry(patch_index, patch)) {
                    label = patch.name;
                }
            } else {
                label = module_at(selection.module_mask, index).name;
            }
        }
        if (selected) display::button(0, menu_top + i * menu_row_height, display::width, menu_row_height, label, 2, black, accent);
        else display::text(0, menu_top + i * menu_row_height, display::width, menu_row_height, label, 2, white, black);
    }
    if (patch_menu && !count)
        display::text(0, menu_top, display::width, menu_row_height, "No patches", 2, white, black);
}

void draw_save_patch(const Selection& selection)
{
    PatchEntry patch{};
    if (selection.active_patch >= 0) {
        patches->entry(selection.active_patch, patch);
    }
    const char* status = selection.active_patch < 0 ? "No active patch" : "Press center to save";
    switch (selection.save_status) {
        case SaveStatus::saved: status = "Patch saved"; break;
        case SaveStatus::changed: status = "Reload patch"; break;
        case SaveStatus::error: status = "Save failed"; break;
        case SaveStatus::none: break;
    }
    display::text(0, 52, display::width, 40, patch.name, 2, white, black);
    display::text(0, 92, display::width, 98, status, 2, white, black);
    display::button(0, 190, display::width, 50, selection.dirty ? "Save patch *" : "Save patch",
                  2, black, accent);
}

void draw_filter(const Selection& selection, bool entering)
{
    static FilterPlot plot;
    static std::array<uint8_t, display::width> previous_points{};
    static std::array<int, 4> previous_parameters{};
    static int previous_selected = -1;
    static std::array<char, 64> previous_caption{};
    static Filter::Response response;
    const auto now = xTaskGetTickCount();
    if (!entering && now - last_filter_refresh < filter_refresh_interval) return;
    last_filter_refresh = now;
    const auto module = selected_module(selection);
    std::array<int, 4> values{};
    for (unsigned i = 0; i < values.size(); ++i) values[i] = module.parameters[i].get();
    Filter::Response live;
    const auto before = response;
    const bool animate = audio_load_tenths->load(std::memory_order_relaxed) < 800;
    const bool received = filter_display.read(live);
    if (entering || values != previous_parameters || ((!animate || (received && !live.live)) && response.live)) response = filter_display.base();
    else if (animate && received && live.live) response = live;
    if (animate) filter_display.request();
    if (entering) draw_title("Filter");
    if (entering || response.mode != before.mode || response.stages != before.stages || response.coefficients != before.coefficients) {
        std::array<uint8_t, display::width> points;
        plot.draw(response, points, plot_height);
        if (entering || points != previous_points) display::response_curve(90, plot_height, points);
        previous_points = points;
    }

    const unsigned selected = parameter_index(selection);
    char buffer[64];
    char value_buffer[24];
    auto label = format_value(module.parameters[selected], values[selected], value_buffer);
    // Mode and order remain permanently visible in the small caption as well.
    if (entering || int(selected) != previous_selected || values != previous_parameters)
        display::text(0, 52, display::width, 38, label, 3, white, black);
    if (selection.learning) std::snprintf(buffer, sizeof(buffer), "MIDI Learn: move a CC");
    else {
        const char* modes[] = {"Bypass", "Low-pass", "High-pass", "Band-pass"};
        if (values[0] == 0) std::snprintf(buffer, sizeof(buffer), "Bypass");
        else if (values[0] == 3) std::snprintf(buffer, sizeof(buffer), "%s | %d poles%s", modes[values[0]], 2 + 2 * values[1], response.live ? " | live" : "");
        else std::snprintf(buffer, sizeof(buffer), "%s | %d dB/oct%s", modes[values[0]], 12 + 12 * values[1], response.live ? " | live" : "");
        if (selection.learned_cc >= 0) {
            const auto used = std::strlen(buffer);
            std::snprintf(buffer + used, sizeof(buffer) - used, " | CC %d", selection.learned_cc);
        }
    }
    if (entering || std::strcmp(buffer, previous_caption.data()) != 0) {
        display::text(0, 178, display::width, 12, buffer, 1, white, black);
        std::snprintf(previous_caption.data(), previous_caption.size(), "%s", buffer);
    }
    if (entering || int(selected) != previous_selected) {
        for (unsigned i = 0; i < values.size(); ++i)
            display::button(i * 70, 190, 70, 50, module.parameters[i].name, 2, i == selected ? black : white, i == selected ? accent : tile);
    }
    previous_parameters = values;
    previous_selected = selected;
}

void draw_parameters(const Selection& selection, const Selection& previous, int& previous_value)
{
    const auto module = selected_module(selection);
    const bool same_module = previous.page == Page::parameters && previous.module == selection.module
                             && previous.module_mask == selection.module_mask;
    const unsigned slot = selection.module == settings_module ? max_patch_modules
        : module_slot(selection.module_mask, selection.module);
    const auto oscillator = std::find(oscillator_slots.begin(), oscillator_slots.end(), slot);
    const bool is_osc = oscillator != oscillator_slots.end();
    const bool is_lfo = slot >= lfo && slot <= lfo3;
    if (slot == filter) {
        draw_filter(selection, !same_module);
        return;
    }
    if (on_sample_page(selection)) {
        static std::array<uint8_t, display::width> points;
        static int previous_start = -1, previous_end = -1;
        const bool entering = !same_module || !on_sample_page(previous);
        const bool sampler_page = is_sampler_slot(slot);
        const int start = sampler_page ? module.parameters[Sampler::start].get() : -1;
        const int end = sampler_page ? module.parameters[Sampler::end].get() : -1;
        const bool new_sample = entering || selection.sample_revision != previous.sample_revision;
        if (entering) {
            draw_title(module.name);
        }
        if (new_sample) {
            std::array<char, 32> name{};
            points.fill(plot_height / 2);
            const bool valid = sample_controls.preview(slot - sampler, name, points, plot_height);
            display::text(0, 52, display::width, 38, valid ? name.data() : "No sample", 2, white, black);
        }
        if (new_sample || start != previous_start || end != previous_end) {
            const int first = sampler_page ? start * (display::width - 1) / 100 : -1;
            const int last = sampler_page ? std::max(start, end) * (display::width - 1) / 100 : -1;
            display::waveform(90, plot_height, points, first, last);
        }
        if (new_sample || start != previous_start || end != previous_end ||
            selection.learning != previous.learning || selection.learned_cc != previous.learned_cc) {
            char label[48]{};
            if (selection.learning) std::snprintf(label, sizeof(label), "MIDI Learn: move a CC");
            else if (sampler_page && selection.learned_cc >= 0)
                std::snprintf(label, sizeof(label), "Start %d%% | End %d%% | CC %d", start, end, selection.learned_cc);
            else if (sampler_page) std::snprintf(label, sizeof(label), "Start %d%% | End %d%%", start, end);
            display::text(0, 178, display::width, 12, label, 1, white, black);
        }
        if (entering || selection.parameter != previous.parameter) {
            const int count = sampler_page ? 4 : sample_actions;
            for (int i = 0; i < count; ++i) {
                const bool selected = i == selection.parameter;
                const char* label = i < sample_actions ? (i ? "Play" : "Sample")
                    : module.parameters[module.parameter_at(i - sample_actions)].name;
                display::button(i * display::width / count, 190, display::width / count, 50, label,
                              2, selected ? black : white, selected ? accent : tile);
            }
        }
        previous_start = start; previous_end = end;
        return;
    }
    const int position = selection.parameter - parameter_offset(slot);
    const int previous_position = previous.parameter - parameter_offset(slot);
    const int index = module.parameter_at(position);
    const int previous_index = module.parameter_at(previous_position);
    const int page_break = is_osc ? int(Osc::pitch) : visible_count;
    const auto first_parameter = [is_osc, page_break, &module](int parameter) {
        for (const auto& page : module.pages)
            if (parameter >= int(page.first) && parameter < int(page.first + page.count)) return int(page.first);
        if (parameter < page_break) return 0;
        return is_osc ? (parameter < Osc::unison ? page_break : int(Osc::unison))
                      : parameter / visible_count * visible_count;
    };
    const int first = first_parameter(position);
    const ParameterPage* group = nullptr;
    for (const auto& page : module.pages) if (page.first == unsigned(first)) { group = &page; break; }
    const auto parameter_name = [&](int at) -> std::string_view {
        if (slot == delay && at == 0) return "Time L";
        const std::string_view name = module.parameters[at].name;
        return slot == fm4 && at >= 6 ? name.substr(2) : name;
    };
    const bool is_save = saving_patch(selection);
    const bool redraw_page = !same_module || on_sample_page(previous) || first_parameter(previous_position) != first
                             || saving_patch(previous) != is_save;
    if (redraw_page) {
        draw_title(group && group->name[0] ? group->name : slot == max_patch_modules && first == play_mode ? "Play" : module.name);
    }
    if (is_save) {
        draw_save_patch(selection);
        return;
    }

    const bool show_wave = (is_lfo && !first) || (is_osc && !first);
    const auto& parameter = module.parameters[index];
    const int value = parameter.get();
    std::string_view wave_name;
    if (is_lfo && show_wave) {
        draw_lfo_wave(module, redraw_page);
    } else if (show_wave) {
        wave_name = draw_osc_wave(module, oscillator - oscillator_slots.begin(), redraw_page);
    }
    char midi_label[24]{};
    const int cc = selection.learned_cc;
    if (selection.learning) std::snprintf(midi_label, sizeof(midi_label), "MIDI Learn: move a CC");
    else if (cc >= 0) std::snprintf(midi_label, sizeof(midi_label), "MIDI CC %d", cc);
    if (redraw_page || selection.learning != previous.learning || cc != previous.learned_cc) {
        display::text(0, show_wave ? 178 : 145, display::width, show_wave ? 12 : 45,
                      midi_label, 1, white, black);
    }
    if (redraw_page || selection.parameter != previous.parameter) {
        if (!show_wave) {
            display::text(0, 52, display::width, 38, parameter_name(index), 2, accent, black);
        }
        const int remaining = static_cast<int>(module.visible_count()) - first;
        const int page_size = is_osc ? (first == 0 ? int(Osc::pitch) : first == Osc::pitch ?
            int(Osc::unison - Osc::pitch) : int(Osc::unison_mix - Osc::unison + 1)) : visible_count;
        const int count = std::min(group ? int(group->count) : page_size, remaining);
        for (int i = 0; i < count; ++i) {
            const int at = module.parameter_at(first + i);
            if (!redraw_page && at != previous_index && at != index) {
                continue;
            }
            const int x = i * display::width / count;
            const int w = (i + 1) * display::width / count - x;
            const bool selected = at == index;
            display::button(x, 190, w, 50, parameter_name(at), 2,
                          selected ? black : white, selected ? accent : tile);
        }
    }
    if (redraw_page || selection.parameter != previous.parameter || previous_value != value) {
        char buffer[24];
        auto label = format_value(parameter, value, buffer);
        if (is_sampler_slot(slot) && index == Sampler::root) {
            constexpr const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            std::snprintf(buffer, sizeof(buffer), "%s%d", notes[value % 12], value / 12 - 1);
            label = buffer;
        }
        if (is_osc && selection.parameter == Osc::wave && !wave_name.empty()) {
            label = wave_name;
        }
        if (is_lfo && selection.parameter == Lfo::rate) {
            const int centihertz = value * 5;
            std::snprintf(buffer, sizeof(buffer), "%d.%02d Hz", centihertz / 100, centihertz % 100);
            label = buffer;
        }
        if ((slot == chorus && index == 0) || (slot == rotary && (index == 1 || index == 2))) {
            std::snprintf(buffer, sizeof(buffer), "%d.%02d Hz", value / 100, value % 100);
            label = buffer;
        }
        display::text(0, show_wave ? 52 : 90, display::width, show_wave ? 38 : 55, label,
                      show_wave ? 3 : 4, white, black);
        previous_value = value;
    }
}

void draw_sample_library(const Selection& selection, bool entering)
{
    static SampleLibraryStatus previous;
    static int previous_action = -1;
    static bool previous_confirmation = false;
    static SampleDeleteStatus previous_deletion = SampleDeleteStatus::none;
    const auto status = sample_library.status();
    if (entering) {
        draw_title("Samples");
    }
    if (entering || previous.revision != status.revision) {
        std::array<char, 32> name{};
        std::array<uint8_t, display::width> points;
        points.fill(plot_height / 2);
        const bool valid = sample_library.preview(name, points, plot_height);
        display::text(0, 52, display::width, 38, valid ? name.data() : "No sample", 2, white, black);
        display::waveform(90, plot_height, points);
    }
    if (entering || status.recording != previous.recording || status.milliseconds != previous.milliseconds ||
        status.revision != previous.revision || status.count != previous.count ||
        previous_action != selection.parameter || previous_confirmation != selection.confirm_sample_delete ||
        previous_deletion != selection.sample_delete_status) {
        char label[48];
        std::snprintf(label, sizeof(label), "%u/%u  %u.%us | Input L: max 5s", status.index, status.count,
            status.duration_ms / 1000, status.duration_ms % 1000 / 100);
        switch (status.recording) {
            case RecordingState::armed: case RecordingState::recording:
                std::snprintf(label, sizeof(label), "Recording %u.%u / 5s", status.milliseconds / 1000,
                    status.milliseconds % 1000 / 100); break;
            case RecordingState::ready: case RecordingState::processing: case RecordingState::saving:
                std::snprintf(label, sizeof(label), "Traitement en cours..."); break;
            case RecordingState::saved: std::snprintf(label, sizeof(label), "Saved | %u.%us | %u samples",
                status.duration_ms / 1000, status.duration_ms % 1000 / 100, status.count); break;
            case RecordingState::no_memory: std::snprintf(label, sizeof(label), "Not enough PSRAM"); break;
            case RecordingState::input_error: std::snprintf(label, sizeof(label), "Audio input error"); break;
            case RecordingState::save_error: std::snprintf(label, sizeof(label), "Save failed: retry"); break;
            case RecordingState::storage_busy: std::snprintf(label, sizeof(label), "USB upload busy: retry"); break;
            case RecordingState::no_storage: std::snprintf(label, sizeof(label), "Sample storage unavailable"); break;
            default: break;
        }
        if (selection.confirm_sample_delete) std::snprintf(label, sizeof(label), "Center: delete | arrows: cancel");
        else switch (selection.sample_delete_status) {
            case SampleDeleteStatus::deleted: std::snprintf(label, sizeof(label), "Sample deleted"); break;
            case SampleDeleteStatus::in_use: std::snprintf(label, sizeof(label), "Used by a patch: cannot delete"); break;
            case SampleDeleteStatus::busy: std::snprintf(label, sizeof(label), "Recording/upload busy"); break;
            case SampleDeleteStatus::changed: std::snprintf(label, sizeof(label), "Sample changed: retry"); break;
            case SampleDeleteStatus::error: std::snprintf(label, sizeof(label), "Delete failed"); break;
            default: break;
        }
        display::text(0, 178, display::width, 12, label, 1, white, black);
    }
    if (entering || previous_action != selection.parameter || status.recording != previous.recording) {
        const bool capturing = status.recording == RecordingState::recording || status.recording == RecordingState::armed;
        const bool saving = status.recording == RecordingState::saving || status.recording == RecordingState::processing ||
            status.recording == RecordingState::ready;
        const char* record = capturing ? "Stop" : saving ? "Saving" :
            status.recording == RecordingState::save_error || status.recording == RecordingState::storage_busy ? "Retry" : "Record";
        const char* labels[] = {"Sample", "Play", record, "Delete"};
        for (int i = 0; i < 4; ++i) {
            const bool selected = selection.parameter == i;
            display::button(i * display::width / 4, 190, display::width / 4, 50, labels[i], 2,
                selected ? black : white, selected ? accent : tile);
        }
    }
    previous = status; previous_action = selection.parameter;
    previous_confirmation = selection.confirm_sample_delete;
    previous_deletion = selection.sample_delete_status;
}

void display_task(void*)
{
    display::init();
    Selection previous;
    previous.module = -2;
    Selection selection = initial_selection;
    bool processing_notice = false;
    int previous_value = 0;
    unsigned displayed_load = ~0u;
    TickType_t last_refresh = xTaskGetTickCount() - load_refresh_interval;
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        if (now - last_refresh >= load_refresh_interval) {
            last_refresh = now;
            const unsigned load = std::min(audio_load_tenths->load(std::memory_order_relaxed),
                                           9999u);
            if (load != displayed_load) {
                draw_load(load);
                displayed_load = load;
            }
        }

        Selection next;
        TickType_t wait = std::min(load_refresh_interval - (now - last_refresh), pdMS_TO_TICKS(100));
        if (!processing_notice && selection.page == Page::parameters && selected_slot(selection) == filter) {
            const auto elapsed = now - last_filter_refresh;
            wait = std::min(wait, elapsed >= filter_refresh_interval ? TickType_t{0} : filter_refresh_interval - elapsed);
        }
        const bool changed = xQueueReceive(selection_updates, &next, wait) == pdTRUE;
        if (changed) selection = next;
        const auto state = sample_library.recording(); // Atomic state only; no flash or sample loading.
        const bool processing = state == RecordingState::ready || state == RecordingState::processing ||
            state == RecordingState::saving;
        if (processing) {
            if (!processing_notice) {
                display::text(0, 52, display::width, 72, "Traitement en cours", 2, white, tile);
                display::text(0, 124, display::width, 66, "Sauvegarde en flash", 2, white, tile);
                display::text(0, 190, display::width, 50, "Ne pas eteindre", 2, accent, tile);
            }
            processing_notice = true;
            continue;
        }
        const bool restore = processing_notice;
        if (restore) {
            previous.page = Page::modules;
            previous.module = -2;
            processing_notice = false;
        }
        if (!changed && !restore) {
            if (previous.page == Page::samples) draw_sample_library(previous, false);
            else if (selection.page == Page::parameters && selected_slot(selection) == filter) draw_filter(selection, false);
            continue;
        }
        if (selection.page == Page::samples) {
            draw_sample_library(selection, previous.page != Page::samples);
        } else if (selection.page != Page::parameters) {
            draw_menu(selection);
        } else {
            draw_parameters(selection, previous, previous_value);
        }
        previous = selection;
    }
}

}  // namespace

void post_midi_cc(uint8_t number, uint8_t value)
{
    const ControlChange cc{number, value};
    xQueueSend(control_changes, &cc, 0);
}

bool post_control_input(PanelInput input)
{
    return remote_controls && xQueueSend(remote_controls, &input, 0) == pdTRUE;
}

void start_controls(std::span<const ModuleParameters> available_modules,
                    const std::atomic<unsigned>& load_tenths,
                    const PatchStore& available_patches, ModuleMask (*load)(unsigned),
                    int (*save)(unsigned), WaveControls waves, SampleControls samples,
                    SampleLibraryControls library, FilterDisplayControls filter_controls, MidiLearnMap& learn,
                    int initial_patch, ModuleMask initial_module_mask)
{
    modules = available_modules;
    patches = &available_patches;
    load_patch = load;
    save_patch = save;
    wave_controls = waves;
    sample_controls = samples;
    sample_library = library;
    filter_display = filter_controls;
    midi_learn = &learn;
    initial_selection.active_patch = initial_patch;
    initial_selection.module_mask = editable_modules(initial_module_mask);
    initial_selection.module = initial_selection.module_mask ? 0 : settings_module;
    audio_load_tenths = &load_tenths;
    display::init_mirror();
    selection_updates = xQueueCreate(1, sizeof(Selection));
    remote_controls = xQueueCreate(16, sizeof(PanelInput));
    control_changes = xQueueCreate(32, sizeof(ControlChange));
    ESP_ERROR_CHECK(selection_updates && control_changes && remote_controls ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(input_task, "controls", 6144, nullptr, 5, nullptr, 0)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(display_task, "display", 6144, nullptr, 3, nullptr, 0)
                    == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
