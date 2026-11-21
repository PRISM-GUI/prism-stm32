#pragma once

// The ADC on Arduino A0 (PA4, ADC1 channel 18) as a prism_examples::SampleSource: TIM3 triggers
// one 16-bit conversion per sample, 16000 times a second, and the DMA writes them into a
// circular buffer. Its half- and full-transfer interrupts convert the finished half to floats in
// [-1, 1) (mid-scale is 0) and push them into a lock-free ring that the logic thread drains.
// With nothing wired to A0 the input floats: touching it shows mains hum (50 or 60 Hz).

#include <embedded/sample_source.hpp>

#include <prism/dev/spsc_ring.hpp>

class AdcSource final : public prism_examples::SampleSource {
public:
    static constexpr float kSampleRate = 16000.f;

    // Starts sampling; false when the ADC, timer or DMA refuse (the firmware then shows the
    // synthetic signal instead). Call once.
    bool init();

    std::size_t read(std::span<float> out) override { return ring_.pop(out); }
    float sample_rate() const override { return kSampleRate; }
    const char *name() const override { return "A0"; }

    // From the DMA interrupt: a finished half-buffer, converted.
    void push(std::span<const float> samples) { ring_.push(samples); }

private:
    prism::dev::SpscRing<float, 4096> ring_; // 256 ms at 16 kHz: the logic thread drains every ~16 ms
};
