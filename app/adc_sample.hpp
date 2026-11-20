#pragma once

// A 16-bit ADC reading (0 .. 65535, unsigned, 0 V .. VREF) as an audio sample in [-1, 1):
// mid-scale is 0, so an input biased at VREF/2 is centred, as an AC-coupled scope shows it.

#include <cstddef>
#include <cstdint>
#include <span>

namespace app {

constexpr float adc_to_sample(std::uint16_t raw) { return (float(raw) - 32768.f) / 32768.f; }

inline void adc_to_samples(std::span<const std::uint16_t> raw, std::span<float> out)
{
    for (std::size_t i = 0; i < raw.size() && i < out.size(); ++i)
        out[i] = adc_to_sample(raw[i]);
}

} // namespace app
