#pragma once

// A stream of mono samples in [-1, 1] at a fixed rate: the board's ADC on Arduino A0, or a
// generated signal where there are none (Renode, host).

#include <cstddef>
#include <span>

namespace app {

struct SampleSource {
    // Non-blocking: copies up to out.size() samples that arrived since the last call and
    // returns how many.
    virtual std::size_t read(std::span<float> out) = 0;
    virtual float sample_rate() const = 0;
    virtual const char *name() const = 0; // shown in the title: "A0", "synthetic"
    virtual ~SampleSource() = default;
};

} // namespace app
