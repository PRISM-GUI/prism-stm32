// Display bring-up and page-flip for the STM32F746G-DISCO (see display.hpp).
#include "display.hpp"

#include "rk043fn48h.h"
#include "stm32746g_discovery.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_ltdc.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <utility>

namespace {
constexpr std::uint32_t kFront = 0xC0000000; // == SDRAM_DEVICE_ADDR and LCD_FB_START_ADDRESS
constexpr std::uint32_t kBack  = 0xC0080000; // right after one 480*272*4-byte buffer
constexpr std::uint32_t kReloadTimeoutMs = 100;
constexpr std::uint32_t kDma2dTimeoutMs = 20;   // a full frame is ~4 ms of SDRAM writes
constexpr std::uint32_t kCacheLine = 32;
constexpr std::uint32_t kDma2dIrqPriority = 15;   // may call FromISR APIs (FreeRTOSConfig.h)

// The panel's display-enable and backlight switches (ST's BSP has them in its LCD header, which
// this firmware does not use: it drives the LTDC itself).
GPIO_TypeDef* const kDispPort = GPIOI;
constexpr std::uint16_t kDispPin = GPIO_PIN_12;
GPIO_TypeDef* const kBlPort = GPIOK;
constexpr std::uint16_t kBlPin = GPIO_PIN_3;

// The board's LTDC (ST's BSP keeps its own, private to its LCD driver, which is not built here).
LTDC_HandleTypeDef g_ltdc;
SemaphoreHandle_t g_reload_done;
#ifndef PRISM_RENODE
SemaphoreHandle_t g_dma2d_done;
volatile std::uint32_t g_dma2d_error;
#endif

// ST's clock values for this panel (BSP_LCD_ClockConfig): PLLSAI VCO = 25 MHz / 25 * 192 = 192 MHz,
// PLLLCDCLK = 192 / 5 = 38.4 MHz, pixel clock = 38.4 / 4 = 9.6 MHz.
void configure_pixel_clock()
{
    RCC_PeriphCLKInitTypeDef clk{};
    clk.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    clk.PLLSAI.PLLSAIN = 192;
    clk.PLLSAI.PLLSAIR = RK043FN48H_FREQUENCY_DIVIDER;
    clk.PLLSAIDivR = RCC_PLLSAIDIVR_4;
    // Renode's RCC model has no PLLSAI: the call times out and the emulated LTDC, which has no
    // clock input, does not care.
    (void)HAL_RCCEx_PeriphCLKConfig(&clk);
}

// The panel's parallel RGB pins (ST's BSP_LCD_MspInit), plus the two plain GPIOs it needs: the
// panel's DISP enable and the backlight switch.
void init_lcd_pins()
{
    __HAL_RCC_LTDC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    GPIO_InitTypeDef pin{};
    pin.Mode = GPIO_MODE_AF_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FAST;

    pin.Pin = GPIO_PIN_4;
    pin.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOE, &pin);

    pin.Pin = GPIO_PIN_12;
    pin.Alternate = GPIO_AF9_LTDC;
    HAL_GPIO_Init(GPIOG, &pin);

    pin.Pin = GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
    pin.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOI, &pin);

    pin.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6
              | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13
              | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOJ, &pin);

    pin.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOK, &pin);

    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FAST;
    pin.Pin = kDispPin;
    HAL_GPIO_Init(kDispPort, &pin);
    pin.Pin = kBlPin;
    HAL_GPIO_Init(kBlPort, &pin);
}

bool init_ltdc()
{
    g_ltdc.Instance = LTDC;

    // RK043FN48H timings, as ST's BSP_LCD_Init writes them: the registers count the sync pulses
    // from zero and accumulate each porch onto the ones before it.
    g_ltdc.Init.HorizontalSync = RK043FN48H_HSYNC - 1;
    g_ltdc.Init.VerticalSync = RK043FN48H_VSYNC - 1;
    g_ltdc.Init.AccumulatedHBP = RK043FN48H_HSYNC + RK043FN48H_HBP - 1;
    g_ltdc.Init.AccumulatedVBP = RK043FN48H_VSYNC + RK043FN48H_VBP - 1;
    g_ltdc.Init.AccumulatedActiveH = RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP - 1;
    g_ltdc.Init.AccumulatedActiveW = RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP - 1;
    g_ltdc.Init.TotalHeigh = RK043FN48H_HEIGHT + RK043FN48H_VSYNC + RK043FN48H_VBP + RK043FN48H_VFP - 1;
    g_ltdc.Init.TotalWidth = RK043FN48H_WIDTH + RK043FN48H_HSYNC + RK043FN48H_HBP + RK043FN48H_HFP - 1;

    g_ltdc.Init.Backcolor.Red = 0;
    g_ltdc.Init.Backcolor.Green = 0;
    g_ltdc.Init.Backcolor.Blue = 0;
    g_ltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
    g_ltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    g_ltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    g_ltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

    if (HAL_LTDC_Init(&g_ltdc) != HAL_OK)
        return false;

    LTDC_LayerCfgTypeDef layer{};
    layer.WindowX0 = 0;
    layer.WindowX1 = Display::kWidth;
    layer.WindowY0 = 0;
    layer.WindowY1 = Display::kHeight;
    layer.PixelFormat = LTDC_PIXEL_FORMAT_ARGB8888;
    layer.FBStartAdress = kFront;
    layer.Alpha = 255;
    layer.Alpha0 = 0;
    layer.Backcolor.Red = 0;
    layer.Backcolor.Green = 0;
    layer.Backcolor.Blue = 0;
    layer.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    layer.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    layer.ImageWidth = Display::kWidth;
    layer.ImageHeight = Display::kHeight;
    if (HAL_LTDC_ConfigLayer(&g_ltdc, &layer, 0) != HAL_OK)
        return false;

    // Both were configured as outputs above: assert the panel's display enable, then the backlight.
    HAL_GPIO_WritePin(kDispPort, kDispPin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(kBlPort, kBlPin, GPIO_PIN_SET);
    return true;
}

} // namespace

// The LTDC applied the register reload (at the vertical blanking): wake present(). In the
// emulated build present() polls the reload flag instead (Renode's LTDC does not raise the
// interrupt), but the NVIC line is enabled there too, so the flag must still be acknowledged:
// an unhandled LTDC interrupt would land in the startup file's default handler and hang.
extern "C" void LTDC_IRQHandler(void)
{
    if ((LTDC->ISR & LTDC_ISR_RRIF) != 0) {
        LTDC->ICR = LTDC_ICR_CRRIF;
#ifndef PRISM_RENODE
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(g_reload_done, &woken);
        portYIELD_FROM_ISR(woken);
#endif
    }
}

#ifndef PRISM_RENODE
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
#endif

bool Display::init()
{
    // The framebuffers are in SDRAM, and the SDRAM is already up: sdram_early.cpp brings the FMC
    // and the MT48LC4M32B2 up from .preinit_array, before any static constructor and before main
    // (both heaps live there). Renode maps the SDRAM as plain memory, so nothing to do there.
    configure_pixel_clock();
    init_lcd_pins();
    if (!init_ltdc())
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
    g_reload_done = xSemaphoreCreateBinary();
    if (g_reload_done == nullptr)
        return false;
    HAL_NVIC_SetPriority(LTDC_IRQn, kDma2dIrqPriority, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
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
    return false;   // the emulated build keeps the CPU clear: Renode's DMA2D would need its own checks
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
    // The MPU maps SDRAM write-through (MPU_Config, main.cpp), so the LTDC sees the writes
    // without a cache clean and this is redundant — kept anyway: if the MPU setup ever changes
    // to write-back, a missing clean shows as tearing/stale pixels.
    SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t *>(pixels.data()),
                            static_cast<int32_t>(pixels.size() * sizeof(std::uint32_t)));

    // Point layer 0 at the back buffer without reloading yet, then request a vertical-blanking
    // reload: the shadow registers (incl. the framebuffer address) are applied at the next
    // vblank, so the flip never tears.
    HAL_LTDC_SetAddress_NoReload(&g_ltdc, back_, 0);
#ifdef PRISM_RENODE
    // Renode's LTDC model does not raise the reload interrupt: poll the flag the hardware clears.
    HAL_LTDC_Reload(&g_ltdc, LTDC_RELOAD_VERTICAL_BLANKING);
    const uint32_t deadline = HAL_GetTick() + kReloadTimeoutMs;
    while ((LTDC->SRCR & LTDC_SRCR_VBR) != 0 &&
           static_cast<int32_t>(HAL_GetTick() - deadline) < 0)
    {
    }
#else
    LTDC->ICR = LTDC_ICR_CRRIF;
    LTDC->IER |= LTDC_IER_RRIE;                 // interrupt when the reload is applied
    xSemaphoreTake(g_reload_done, 0);           // a stale give from a reload that timed out
    HAL_LTDC_Reload(&g_ltdc, LTDC_RELOAD_VERTICAL_BLANKING);

    // Sleep until the reload has been applied (the interrupt gives the semaphore): the frame
    // waits up to a whole refresh here, and the CPU is the logic thread's (or idle) meanwhile.
    // A timeout, so a stuck LTDC cannot hang the render thread forever.
    xSemaphoreTake(g_reload_done, pdMS_TO_TICKS(kReloadTimeoutMs));
#endif

    std::swap(front_, back_);
}
