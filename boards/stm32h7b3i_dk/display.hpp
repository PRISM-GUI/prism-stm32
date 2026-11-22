#pragma once

// The board's display: the RK043FN48H 480x272 LCD driven by the LTDC, with double buffering
// in SDRAM (0xD0000000 front = LCD_LAYER_0_ADDRESS where BSP_LCD_InitEx puts layer 0, and
// 0xD0080000 back — 480*272*4 = 522240 bytes each). Implements PRISM's FrameSink
// (prism/platform/frame_target.hpp): the render thread draws into back(), then present()
// flips the LTDC layer to it.

#include <prism/platform/frame_target.hpp>

#include <cstdint>

class Display final : public prism::platform::FrameSink {
public:
    static constexpr int kWidth = 480;
    static constexpr int kHeight = 272;

    // SDRAM first (the framebuffers live there), then the LCD: LTDC + layer 0 at the front
    // buffer, ARGB8888 (BSP_LCD_InitEx's LCD_PIXEL_FORMAT_RGB888 maps to LTDC ARGB8888),
    // backlight and display enable on (BSP_LCD_InitEx asserts the pins; BSP_LCD_DisplayOn
    // sets the DISP pin). False on failure — the caller parks.
    bool init();

    // The back buffer, as ARGB8888 pixels (the rasterizer's Canvas format), stride = width.
    std::span<std::uint32_t> back() override;

    // Clean the D-cache for the back buffer, point LTDC layer 0 at it (no reload yet),
    // request a vertical-blanking reload, wait for the reload flag to clear (with a
    // timeout), then swap front/back.
    void present() override;

    prism::core::Size size() const override
    {
        return {prism::core::Width{float(kWidth)}, prism::core::Height{float(kHeight)}};
    }

    // Fills a rectangle of the back buffer with the DMA2D (Chrom-ART, register to memory) and waits
    // for it; the CPU's cache is invalidated over the rectangle afterwards, so the rasterizer's
    // blending reads what the DMA2D wrote. False (the CPU clears) where there is no DMA2D (Renode).
    bool clear_rect(int x0, int y0, int x1, int y1, std::uint32_t argb) override;

    // Two buffers that swap: the back one shows the frame before last, so the runtime redraws
    // only what changed in the last two frames (PRISM's damage tracking).
    int buffer_age() const override { return 2; }

private:
#ifdef PRISM_RENODE
    void fit_active_width_to_layer();
#endif
    std::uint32_t front_ = 0xD0000000;
    std::uint32_t back_ = 0xD0080000;
};
