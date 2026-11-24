#include "synth_voice.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>

static void set(ModuleParameters c, const char* name, int value)
{
    for (auto& p : c.parameters) if (std::string_view(p.name) == name) {
        p.set(value); if (c.on_change) c.on_change(c.context); return;
    }
}
int main()
{
    for (int live = 0; live < 2; ++live) {
        double times[7]{};
        uint64_t expected = 0;
        for (unsigned pass = 0; pass < 7; ++pass) {
            PatchSettings settings;
            settings.parameters[0].set(8);
            auto synth = std::make_unique<SynthVoice>(settings);
            synth->prepare();
            set(synth->filter_controls(), "Mode", 1);
            set(synth->filter_controls(), "Order", 1);
            set(synth->filter_controls(), "Cutoff", 1800);
            set(synth->filter_controls(), "Reso", 65);
            auto plan = default_audio_plan();
            plan.modules |= module_bit(lfo);
            plan.modulation |= uint64_t{1} << cutoff_port;
            plan.sources[cutoff_port] = lfo;
            synth->select_plan(plan);
            for (unsigned note = 0; note < 8; ++note) synth->note_on(1, 0, 48 + note, 90);
            std::array<int16_t, 64> buffer{};
            uint64_t checksum = 0;
            const auto start = std::chrono::steady_clock::now();
            for (unsigned block = 0; block < 25000; ++block) {
#ifdef PREVIEW_BENCH
                if (live && block % 138 == 0 && synth->request_filter_response()) synth->publish_filter_response();
#endif
                synth->render(buffer.data());
#ifdef PREVIEW_BENCH
                if (live && block % 138 == 1) { Filter::Response response; synth->read_filter_response(response); }
#endif
                checksum = checksum * 33 + uint16_t(buffer[0]);
            }
            times[pass] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (!pass) expected = checksum;
            else if (checksum != expected) return 1;
        }
        std::sort(std::begin(times), std::end(times));
        std::printf("preview=%d median_ms=%.3f checksum=%llu\n", live, times[3], (unsigned long long)expected);
    }
}
