#include "display_painter.h"
#include "filter_plot.h"
#include "synth_voice.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <thread>

namespace classic {
#define PROGMEM
#include "../../libraries/TFT_eSPI/Fonts/glcdfont.c"
#undef PROGMEM
}

static void set(ModuleParameters c, const char* name, int value)
{
    for (auto& p : c.parameters) if (std::string_view(p.name) == name) {
        p.set(value); if (c.on_change) c.on_change(c.context); return;
    }
    assert(false);
}
using Frame = std::array<uint16_t, 280 * 240>;
static void caption(Frame& frame, std::string_view label)
{
    label = label.substr(0, 280 / 6);
    const int start = (280 - int(label.size()) * 6) / 2;
    for (unsigned c = 0; c < label.size(); ++c)
        for (int x = 0; x < 5; ++x)
            for (int y = 0; y < 7; ++y)
                if (classic::font[5 * uint8_t(label[c]) + x] & (1 << y)) frame[(180 + y) * 280 + start + c * 6 + x] = 0xffff;
}
static void text(Frame& frame, int x, int y, int w, int h, std::string_view label, int size,
                 uint16_t ink, uint16_t bg, display::TextStyle style = display::TextStyle::flat)
{
    display::TextRaster raster(w, h, label, size, ink, bg, style);
    for (int row = 0; row < h; ++row) raster.row(row, {frame.data() + (y + row) * 280 + x, unsigned(w)});
}
static void image(const char* file, const Filter::Response& response, const char* value)
{
    Frame frame{};
    text(frame, 0, 0, 280, 50, "Filter", 3, 0xffff, 0x39c7, display::TextStyle::heading);
    text(frame, 0, 52, 280, 38, value, 3, 0xffff, 0);
    FilterPlot plot;
    std::array<uint8_t, 280> points;
    plot.draw(response, points, 88);
    for (unsigned row = 0; row < 88; ++row)
        display::paint_response_row({frame.data() + (90 + row) * 280, 280}, points, row, 88);
    const char* modes[] = {"Bypass", "Low-pass", "High-pass", "Band-pass"};
    char caption[40];
    if (response.mode == 3) std::snprintf(caption, sizeof(caption), "%s | %d poles", modes[response.mode], response.stages * 2);
    else std::snprintf(caption, sizeof(caption), "%s | %d dB/oct", modes[response.mode], response.stages * 12);
    ::caption(frame, caption);
    const char* labels[] = {"Mode", "Order", "Cutoff", "Reso"};
    for (int i = 0; i < 4; ++i)
        text(frame, 70 * i, 190, 70, 50, labels[i], 2, i == 2 ? 0 : 0xffff, i == 2 ? 0x3fff : 0x39c7, display::TextStyle::button);
    std::ofstream out(file, std::ios::binary);
    out << "P6\n280 240\n255\n";
    for (auto color : frame) {
        const char rgb[] = {char(((color >> 11) & 31) * 255 / 31), char(((color >> 5) & 63) * 255 / 63), char((color & 31) * 255 / 31)};
        out.write(rgb, 3);
    }
}
int main()
{
    Filter filter{44100};
    const auto c = filter.controls();
    FilterPlot plot;
    std::array<uint8_t, 280> low, high, band, steep, resonant, moved, bypass;
    set(c, "Cutoff", 1200); set(c, "Reso", 0); set(c, "Mode", 1);
    plot.draw(filter.base_response(), low, 88);
    image("build/filter-low.ppm", filter.base_response(), "1200 Hz");
    assert(low.front() < low.back());
    set(c, "Order", 1);
    plot.draw(filter.base_response(), steep, 88);
    assert(steep[230] >= low[230]);
    set(c, "Reso", 85);
    plot.draw(filter.base_response(), resonant, 88);
    assert(*std::min_element(resonant.begin(), resonant.end()) < *std::min_element(steep.begin(), steep.end()));
    image("build/filter-resonant.ppm", filter.base_response(), "1200 Hz");
    set(c, "Cutoff", 5000);
    plot.draw(filter.base_response(), moved, 88);
    assert(moved[200] < resonant[200]);
    set(c, "Mode", 2);
    plot.draw(filter.base_response(), high, 88);
    assert(high.front() > high.back());
    image("build/filter-high.ppm", filter.base_response(), "5000 Hz");
    set(c, "Mode", 3);
    plot.draw(filter.base_response(), band, 88);
    assert(*std::min_element(band.begin() + 20, band.end() - 20) < band.front());
    image("build/filter-band.ppm", filter.base_response(), "5000 Hz");
    set(c, "Mode", 0);
    plot.draw(filter.base_response(), bypass, 88);
    assert(std::all_of(bypass.begin(), bypass.end(), [&](auto y) { return y == bypass[0]; }));

    // Real DSP: snapshots must not change even a single output sample.
    for (bool envelope : {false, true}) {
        PatchSettings settings;
        settings.parameters[polyphony].set(4);
        auto reference = std::make_unique<SynthVoice>(settings), preview = std::make_unique<SynthVoice>(settings);
        auto plan = default_audio_plan();
        plan.modules |= module_bit(lfo) | (envelope ? module_bit(env1) : 0);
        plan.modulation = (uint64_t{1} << cutoff_port) | (uint64_t{1} << resonance_port);
        plan.sources[cutoff_port] = envelope ? env1 : lfo;
        plan.sources[resonance_port] = lfo;
        for (auto* synth : {reference.get(), preview.get()}) {
            synth->prepare();
            set(synth->filter_controls(), "Mode", 1);
            set(synth->filter_controls(), "Order", 1);
            set(synth->filter_controls(), "Cutoff", 700);
            set(synth->lfo_controls(), "Rate", 30);
            set(synth->env_controls(0), "Attack", 600);
            synth->select_plan(plan);
            for (int n = 0; n < 4; ++n) synth->note_on(1, 0, 48 + 3 * n, 100);
        }
        std::array<int16_t, 64> a{}, b{};
        Filter::Response previous, response;
        unsigned changes = 0;
        for (unsigned block = 0; block < 3000; ++block) {
            reference->render(a.data()); preview->render(b.data());
            assert(a == b);
            if (block % 138 == 0) {
                assert(preview->request_filter_response());
                preview->publish_filter_response(); // Same audio-core command as the firmware.
                assert(preview->read_filter_response(response));
                assert(response.live && response.stages == 2);
                if (response.coefficients != previous.coefficients) ++changes;
                previous = response;
            }
        }
        assert(changes >= 10);
    }
    // Cross-core mailbox: a ready response must never contain a torn payload.
    struct Payload { unsigned sequence; std::array<unsigned, 32> values; };
    DisplayMailbox<Payload> mailbox;
    std::atomic<bool> done{false};
    std::thread audio([&] {
        unsigned sequence = 0;
        while (!done.load()) if (mailbox.wanted()) {
            Payload payload; payload.sequence = ++sequence; payload.values.fill(sequence);
            mailbox.publish(payload);
        }
    });
    for (unsigned i = 0; i < 20000; ++i) {
        assert(mailbox.request());
        Payload payload;
        while (!mailbox.read(payload)) std::this_thread::yield();
        assert(std::all_of(payload.values.begin(), payload.values.end(), [&](unsigned x) { return x == payload.sequence; }));
    }
    done.store(true); audio.join();

    std::array<uint16_t, 282> row;
    display::TextRaster font(280, 55, "1234 Hz", 4, 0xffff, 0);
    unsigned antialiased = 0;
    for (unsigned y = 0; y < 55; ++y) {
        row.fill(0x1234);
        font.row(y, {row.data() + 1, 280});
        assert(row.front() == 0x1234 && row.back() == 0x1234);
        for (unsigned i = 1; i <= 280; ++i) antialiased += row[i] != 0 && row[i] != 0xffff;
    }
    assert(antialiased > 100);
    std::puts("PASS: LP/HP/BP/bypass, cutoff/resonance/order curves, actual LFO/envelope snapshots, bit-identical 4-voice audio, 20000 concurrent snapshots, native antialiased font bounds");
}
