// Display bring-up and page-flip for the STM32H7B3I-DK (see display.hpp).
#include "display.hpp"

#include "stm32h7b3i_discovery.h"
#include "stm32h7b3i_discovery_lcd.h"
#include "stm32h7b3i_discovery_sdram.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_ltdc.h"

#include <utility>

namespace {
constexpr std::uint32_t kFront = 0xD0000000; // == LCD_LAYER_0_ADDRESS (stm32h7b3i_discovery_conf.h)
constexpr std::uint32_t kBack  = 0xD0080000; // right after one 480*272*4-byte buffer
constexpr std::uint32_t kReloadTimeoutMs = 100;
} // namespace

bool Display::init()
{
    // The framebuffers are plain SDRAM addresses, so the SDRAM must be up before the LCD
    // (BSP_LCD_InitEx would init it itself too — re-initializing the FMC bank before any
    // SDRAM access is harmless; doing it here gives a distinct failure point).
    if (BSP_SDRAM_Init(0) != BSP_ERROR_NONE)
        return false;

    // LCD_PIXEL_FORMAT_RGB888 is the BSP's name for LTDC_PIXEL_FORMAT_ARGB8888, 4 bytes per
    // pixel — the rasterizer's Canvas format. Landscape, 480x272, layer 0 at the front buffer.
    if (BSP_LCD_InitEx(0, LCD_ORIENTATION_LANDSCAPE, LCD_PIXEL_FORMAT_RGB888, kWidth, kHeight) != BSP_ERROR_NONE)
        return false;

    // Backlight was asserted by BSP_LCD_InitEx; turn the display itself on.
    if (BSP_LCD_DisplayOn(0) != BSP_ERROR_NONE)
        return false;

#ifdef PRISM_RENODE
    fit_active_width_to_layer();
#endif
    front_ = kFront;
    back_ = kBack;
    return true;
}

#ifdef PRISM_RENODE
// ST's timings for this panel give an active width of 491 pixels around a 480-pixel layer
// (the real LTDC shows the layer and background beyond it). Renode's LTDC model reads the
// framebuffer with the active width as its line length, which shears the picture, so the
// emulated build narrows the active area to the layer's width. Renode only.
void Display::fit_active_width_to_layer()
{
    const std::uint32_t accumulated_hbp = (LTDC->BPCR & LTDC_BPCR_AHBP) >> LTDC_BPCR_AHBP_Pos;
    const std::uint32_t active_height = LTDC->AWCR & LTDC_AWCR_AAH;
    LTDC->AWCR = ((accumulated_hbp + kWidth) << LTDC_AWCR_AAW_Pos) | active_height;
    LTDC->SRCR = LTDC_SRCR_IMR;
}
#endif

std::span<std::uint32_t> Display::back()
{
    return {reinterpret_cast<std::uint32_t *>(back_),
            static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight)};
}

void Display::present()
{
    auto pixels = back();
    // The MPU maps SDRAM write-through (MPU_Config, region 1), so the LTDC sees the
    // writes without a cache clean and this is redundant — kept anyway: if the MPU
    // setup ever changes to write-back, a missing clean shows as tearing/stale pixels.
    // Both buffers are 32-byte aligned and 522240 bytes is a multiple of 32 (the cache
    // line size), as SCB_CleanDCache_by_Addr requires.
    SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t *>(pixels.data()),
                            static_cast<int32_t>(pixels.size() * sizeof(std::uint32_t)));

    // Point layer 0 at the back buffer without reloading yet, then request a
    // vertical-blanking reload: the shadow registers (incl. the framebuffer address)
    // are applied at the next vblank, so the flip never tears.
    HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc, back_, 0);
    HAL_LTDC_Reload(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING);

    // Wait until the reload has been applied (hardware clears the VBR bit in SRCR),
    // with a timeout so a stuck LTDC cannot hang the render thread forever.
    const uint32_t deadline = HAL_GetTick() + kReloadTimeoutMs;
    while ((LTDC->SRCR & LTDC_SRCR_VBR) != 0 &&
           static_cast<int32_t>(HAL_GetTick() - deadline) < 0)
    {
    }

    std::swap(front_, back_);
}
