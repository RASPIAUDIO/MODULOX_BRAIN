#include "recording.h"
#include "sampler.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <string_view>
#include <vector>

int main()
{
    Recording recorder;
    std::vector<int16_t> pcm(Recording::max_frames + 2, -1234);
    recorder.arm({pcm.data(), Recording::max_frames});
    assert(recorder.busy() && recorder.frames() == 0);
    recorder.set(RecordingState::recording);
    std::array<int16_t, 64> input;
    unsigned frame = 0;
    for (;;) {
        for (unsigned i = 0; i < 32; ++i) {
            input[2 * i] = int16_t((frame + i) % 32768);
            input[2 * i + 1] = -30000;
        }
        const bool done = recorder.capture(input);
        frame += 32;
        if (done) break;
    }
    assert(recorder.frames() == 220500); // Exactly five seconds, even with a partial last block.
    for (unsigned i = 0; i < recorder.frames(); ++i) assert(pcm[i] == int16_t(i % 32768));
    assert(pcm[Recording::max_frames] == -1234 && pcm[Recording::max_frames + 1] == -1234);
    recorder.set(RecordingState::ready);
    assert(recorder.busy()); recorder.set(RecordingState::processing); assert(recorder.busy());
    recorder.set(RecordingState::saving); assert(recorder.busy());
    recorder.set(RecordingState::saved); assert(!recorder.busy());

    recorder.arm(pcm);
    recorder.set(RecordingState::recording);
    assert(!recorder.capture(input));
    recorder.stop(); assert(recorder.stopped() && recorder.frames() == 32);
    recorder.set(RecordingState::ready); recorder.set(RecordingState::save_error);
    assert(!recorder.busy() && recorder.frames() == 32); // A failed save retains the take for retry.
    recorder.set(RecordingState::ready); assert(recorder.busy());

    recorder.arm(pcm); assert(!recorder.stopped() && recorder.frames() == 0);
    recorder.stop(); recorder.set(RecordingState::idle); assert(!recorder.busy());
    std::puts("Recording: exact 5s limit, left-channel PCM16, bounds, early stop, save/retry and reuse passed.");

    // Button spikes must be excluded before computing the normalization gain.
    constexpr unsigned first = Recording::sample_rate * Recording::trim_start_ms / 1000;
    constexpr unsigned tail = Recording::sample_rate * Recording::trim_end_ms / 1000;
    std::vector<int16_t> take(Recording::sample_rate, 1000);
    std::fill_n(take.begin(), first, 30000);
    std::fill(take.end() - tail, take.end(), -30000);
    take.push_back(123); take.push_back(456);
    const unsigned count = Recording::prepare({take.data(), Recording::sample_rate});
    assert(count == Recording::sample_rate - first - tail);
    assert(take[0] == 0 && take[count - 1] == 0 && take[100] == 32767);
    assert(take[Recording::sample_rate] == 123 && take[Recording::sample_rate + 1] == 456);
    std::vector<int16_t> negative(Recording::sample_rate, -32768);
    const unsigned n = Recording::prepare(negative);
    assert(negative[n / 2] == -32767);
    std::vector<int16_t> silence(Recording::sample_rate, 0);
    const unsigned quiet = Recording::prepare(silence);
    assert(std::all_of(silence.begin(), silence.begin() + quiet, [](int16_t v) { return v == 0; }));
    for (unsigned length : {0u, 1u, 2u, 3u, 32u, 441u, 4410u}) {
        std::vector<int16_t> short_take(length + 2, 1000);
        short_take[length] = 123; short_take[length + 1] = 456;
        const unsigned kept = Recording::prepare({short_take.data(), length});
        assert(kept <= length && (length < 2 || kept >= length / 2));
        assert(short_take[length] == 123 && short_take[length + 1] == 456);
    }
    std::puts("Recording processing: asymmetric trims, post-trim peak normalization, fades, silence, -32768 and short takes passed.");

    Sampler sampler;
    const auto controls = sampler.controls("Sampler");
    const unsigned order[] = {Sampler::start, Sampler::end, Sampler::root, Sampler::pitch,
        Sampler::fine, Sampler::level, Sampler::mode, Sampler::channel};
    for (unsigned i = 0; i < 8; ++i) {
        assert(controls.parameter_at(i) == order[i]);
        assert(controls.parameter_position(order[i]) == i);
    }
    assert(controls.parameters[5].name == std::string_view("Start"));
    assert(controls.parameters[6].name == std::string_view("End"));
    assert(controls.pages[1].first == 2 && controls.pages[1].count == 4);
    std::puts("Sampler menu: new display order preserves all stored parameter and MIDI Learn indices.");
}
