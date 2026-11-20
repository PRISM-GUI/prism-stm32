#include "mic_source.hpp"

#include "stm32h7b3i_discovery_audio.h"

#include <array>
#include <cstdint>

namespace {

// The BSP fills this with interleaved 16-bit stereo (microphone 1, microphone 2) and calls back
// at each half. Its size is a multiple of what one DFSDM DMA half delivers
// (DEFAULT_AUDIO_IN_BUFFER_SIZE, stm32h7b3i_discovery_conf.h): 4 x that in bytes, as in ST's
// BSP audio example, so each callback covers DEFAULT_AUDIO_IN_BUFFER_SIZE / 2 frames.
constexpr std::size_t kFramesPerHalf = DEFAULT_AUDIO_IN_BUFFER_SIZE / 2;
alignas(32) std::array<std::int16_t, 4 * kFramesPerHalf> g_record;
std::array<float, kFramesPerHalf> g_converted; // used from the DMA interrupt only
MicSource *g_mic = nullptr;

void push_half(std::size_t half)
{
    if (g_mic == nullptr)
        return;
    const std::int16_t *frames = g_record.data() + half * 2 * kFramesPerHalf;
    for (std::size_t i = 0; i < kFramesPerHalf; ++i)
        g_converted[i] = float(frames[2 * i]) / 32768.f; // microphone 1: the even slots
    g_mic->push(g_converted);
}

} // namespace

bool MicSource::init()
{
    // The DFSDM's audio clock (the microphones' bit clock) is SAI1's kernel clock, which the
    // BSP routes from PLL2 but never enables when only recording: ST's own example on this
    // board (Examples/BSP, audio_loopback.c) also starts the SAI1 headphone output, which does.
    // A guess from the first board run (title read "mic", no signal): the title's "S/s" tells.
    __HAL_RCC_SAI1_CLK_ENABLE();
    BSP_AUDIO_Init_t in = {};
    in.Device = AUDIO_IN_DEVICE_DIGITAL_MIC;
    in.SampleRate = static_cast<std::uint32_t>(kSampleRate);
    in.BitsPerSample = AUDIO_RESOLUTION_16B;
    in.ChannelsNbr = 2;
    in.Volume = 100;
    if (BSP_AUDIO_IN_Init(2, &in) != BSP_ERROR_NONE)
        return false;
    g_mic = this;
    return BSP_AUDIO_IN_Record(2, reinterpret_cast<std::uint8_t *>(g_record.data()),
                               sizeof g_record) == BSP_ERROR_NONE;
}

extern "C" void BSP_AUDIO_IN_HalfTransfer_CallBack(uint32_t Instance)
{
    if (Instance == 2U)
        push_half(0);
}

extern "C" void BSP_AUDIO_IN_TransferComplete_CallBack(uint32_t Instance)
{
    if (Instance == 2U)
        push_half(1);
}
