#pragma once

// The board's MEMS microphones as an app::SampleSource: ST's audio BSP (instance 2, DFSDM)
// records both microphones by DMA into 16-bit stereo; its half/complete callbacks run in the
// DMA interrupt and push the selected microphone's half-buffer, as floats in [-1, 1], into a lock-free
// ring that the logic thread drains with read().

#include "app/sample_source.hpp"
#include "app/spsc_ring.hpp"

#include <atomic>

class MicSource final : public app::SampleSource {
public:
    static constexpr float kSampleRate = 16000.f;

    // Starts recording; false when the BSP refuses (the firmware then shows the synthetic
    // signal instead). Call once.
    bool init();

    std::size_t read(std::span<float> out) override { return ring_.pop(out); }
    float sample_rate() const override { return kSampleRate; }
    const char *name() const override { return "mic"; }
    int channel_count() const override { return 2; }
    void select_channel(int channel) override { channel_.store(channel, std::memory_order_relaxed); }

    // From the DMA interrupt: the selected microphone's samples of a finished half-buffer.
    void push(std::span<const float> samples) { ring_.push(samples); }
    int channel() const { return channel_.load(std::memory_order_relaxed); } // 0: MIC1, 1: MIC2

private:
    std::atomic<int> channel_{0};
    app::SpscRing<float, 4096> ring_; // 256 ms at 16 kHz: the logic thread drains every ~16 ms
};
