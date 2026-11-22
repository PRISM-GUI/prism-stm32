// Display bring-up and page-flip for the STM32H7B3I-DK (see display.hpp).
#include "display.hpp"

#include "stm32h7b3i_discovery.h"
#include "stm32h7b3i_discovery_lcd.h"
#include "stm32h7b3i_discovery_sdram.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_ltdc.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <utility>

namespace {
constexpr std::uint32_t kFront = 0xD0000000; // == LCD_LAYER_0_ADDRESS (stm32h7b3i_discovery_conf.h)
constexpr std::uint32_t kBack  = 0xD0080000; // right after one 480*272*4-byte buffer
constexpr std::uint32_t kReloadTimeoutMs = 100;
constexpr std::uint32_t kDma2dTimeoutMs = 20;   // a full frame is ~3 ms of SDRAM writes
constexpr std::uint32_t kCacheLine = 32;
constexpr std::uint32_t kDma2dIrqPriority = 15;   // may call FromISR APIs (FreeRTOSConfig.h)

SemaphoreHandle_t g_dma2d_done;
volatile std::uint32_t g_dma2d_error;
} // namespace

// The DMA2D transfer ended (or failed): wake the thread that is waiting in clear_rect(), which
// sleeps meanwhile and leaves the CPU to the logic thread.
extern "C" void DMA2D_IRQHandler(void)
{
    const std::uint32_t flags = DMA2D->ISR & (DMA2D_ISR_TCIF | DMA2D_ISR_TEIF | DMA2D_ISR_CEIF);
    DMA2D->IFCR = flags;
    g_dma2d_error = flags & (DMA2D_ISR_TEIF | DMA2D_ISR_CEIF);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(g_dma2d_done, &woken);
    portYIELD_FROM_ISR(woken);
}

bool Display::init()
{
    // The framebuffers are plain SDRAM addresses: BSP_LCD_InitEx brings the SDRAM up itself
    // (BSP_SDRAM_Init, as ST's examples rely on) before it configures layer 0, so the SDRAM is
    // not initialized a second time here.
    // LCD_PIXEL_FORMAT_RGB888 is the BSP's name for LTDC_PIXEL_FORMAT_ARGB8888, 4 bytes per
    // pixel — the rasterizer's Canvas format. Landscape, 480x272, layer 0 at the front buffer.
    if (BSP_LCD_InitEx(0, LCD_ORIENTATION_LANDSCAPE, LCD_PIXEL_FORMAT_RGB888, kWidth, kHeight) != BSP_ERROR_NONE)
        return false;

    // Backlight was asserted by BSP_LCD_InitEx; turn the display itself on.
    if (BSP_LCD_DisplayOn(0) != BSP_ERROR_NONE)
        return false;

#ifdef PRISM_RENODE
    fit_active_width_to_layer();
#else
    g_dma2d_done = xSemaphoreCreateBinary();
    if (g_dma2d_done == nullptr)
        return false;
    __HAL_RCC_DMA2D_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2D_IRQn, kDma2dIrqPriority, 0);
    HAL_NVIC_EnableIRQ(DMA2D_IRQn);
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

bool Display::clear_rect(int x0, int y0, int x1, int y1, std::uint32_t argb)
{
#ifdef PRISM_RENODE
    (void)x0; (void)y0; (void)x1; (void)y1; (void)argb;
    return false;   // Renode has no DMA2D model
#else
    const auto w = static_cast<std::uint32_t>(x1 - x0), h = static_cast<std::uint32_t>(y1 - y0);
    if (x1 <= x0 || y1 <= y0)
        return true;
    const std::uint32_t row_bytes = kWidth * sizeof(std::uint32_t);
    const std::uint32_t first = back_ + static_cast<std::uint32_t>(y0) * row_bytes
                                + static_cast<std::uint32_t>(x0) * sizeof(std::uint32_t);
    DMA2D->CR = DMA2D_R2M | DMA2D_CR_TCIE | DMA2D_CR_TEIE | DMA2D_CR_CEIE;   // register to memory
    DMA2D->OPFCCR = DMA2D_OUTPUT_ARGB8888;
    DMA2D->OCOLR = argb;
    DMA2D->OMAR = first;
    DMA2D->OOR = static_cast<std::uint32_t>(kWidth) - w;   // pixels to skip at the end of each line
    DMA2D->NLR = (w << DMA2D_NLR_PL_Pos) | h;
    DMA2D->IFCR = DMA2D_IFCR_CTCIF | DMA2D_IFCR_CTEIF | DMA2D_IFCR_CCEIF;
    g_dma2d_error = 0;
    xSemaphoreTake(g_dma2d_done, 0);   // a stale give from a transfer that timed out
    DMA2D->CR |= DMA2D_CR_START;
    if (xSemaphoreTake(g_dma2d_done, pdMS_TO_TICKS(kDma2dTimeoutMs)) != pdTRUE)
        return false;   // stuck: the CPU clears instead (and the DMA2D is left to finish)
    if (g_dma2d_error != 0)
        return false;
    // The DMA2D wrote around the cache. The MPU maps SDRAM write-through, so the cache holds no
    // dirty line: invalidating whole lines, rectangle edges included, loses nothing.
    const std::uint32_t last = first + (h - 1) * row_bytes + w * sizeof(std::uint32_t);
    const std::uint32_t begin = first & ~(kCacheLine - 1), end = (last + kCacheLine - 1) & ~(kCacheLine - 1);
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t *>(begin), static_cast<int32_t>(end - begin));
    return true;
#endif
}

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
