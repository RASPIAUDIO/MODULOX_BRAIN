#pragma once

#include "patch_store.h"
#include "recording.h"
#include "filter.h"

struct FilterDisplayControls {
    Filter::Response (*base)();
    bool (*read)(Filter::Response& response);
    void (*request)();
};

struct WaveControls {
    int (*maximum)();
    bool (*select)(unsigned oscillator, int wave);
    bool (*preview)(unsigned oscillator, int wave, char (&name)[32], std::span<uint8_t> points, int height);
};

struct SampleControls {
    bool (*step)(unsigned sampler, int direction);
    void (*play)(unsigned sampler);
    bool (*preview)(unsigned sampler, std::span<char, 32> name, std::span<uint8_t> points, int height);
};

struct SampleLibraryControls {
    bool (*step)(int direction);
    void (*play)();
    void (*record)();
    SampleLibraryStatus (*status)();
    bool (*preview)(std::span<char, 32> name, std::span<uint8_t> points, int height);
    RecordingState (*recording)();
    SampleDeleteStatus (*remove)(unsigned expected_revision);
};

void post_midi_cc(uint8_t number, uint8_t value);

enum class PanelInput { left, center, right, menu, encoder_press, clockwise, counterclockwise };
bool post_control_input(PanelInput input);

void start_controls(std::span<const ModuleParameters> modules,
                    const std::atomic<unsigned>& audio_load_tenths,
                    const PatchStore& patches, ModuleMask (*load_patch)(unsigned),
                    int (*save_patch)(unsigned), WaveControls waves, SampleControls samples,
                    SampleLibraryControls library, FilterDisplayControls filter_display, MidiLearnMap& learn,
                    int initial_patch, ModuleMask initial_module_mask);
