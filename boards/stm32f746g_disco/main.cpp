// STM32F746G-DISCO firmware entry point: board bring-up (MPU, caches, HAL, clocks), then one
// FreeRTOS task that runs a PRISM app through PRISM's frame runtime (frame_app) — a logic thread
// and a render thread on top of it. Built twice: stm32f746g_disco.elf for the real board, and
// stm32f746g_disco_renode.elf (-DPRISM_RENODE) for Renode, which differs in display.cpp,
// touch.cpp and this clock setup.
#include "adc_source.hpp"
#include "boot_log.h"
#include "stm32746g_discovery_sdram.h"
#include "display.hpp"
#include "touch.hpp"
#include "uart_stream.hpp"

#include <embedded/audio_scope.hpp>
#include <embedded/synthetic_source.hpp>
#include <embedded/widgets_app.hpp>

#include <prism/platform/alloc_count.hpp>
#include <prism/platform/frame_app.hpp>
#include <prism/profile/dump.hpp>

#include "stm32746g_discovery.h"
#include "stm32746g_discovery_sdram.h"
#include "stm32f7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

// The framebuffers and both heaps live in the SDRAM: map it write-back, cacheable and not shared.
// Not write-through: Cortex-M7 erratum 1259864 (r0p1..r1p1, this board's core is r0p1) lets a
// load from write-through memory miss the last store to it, which corrupted malloc's heap here.
// ARM's workaround is write-back; the display then cleans before the LTDC or the DMA2D reads. Everything else keeps the
// default map: the MPU is enabled with the privileged default region, as in ST's F7 SDRAM
// example — the F7's FMC registers at 0xA0000000 must stay reachable, which a 4 GB background
// region would cut off.
static void MPU_Config(void)
{
    MPU_Region_InitTypeDef region;
    HAL_MPU_Disable();

    region.Enable = MPU_REGION_ENABLE;
    region.Number = MPU_REGION_NUMBER0;
    region.BaseAddress = SDRAM_DEVICE_ADDR;
    region.Size = MPU_REGION_SIZE_16MB;
    region.SubRegionDisable = 0x00;
    region.TypeExtField = MPU_TEX_LEVEL0;
    region.AccessPermission = MPU_REGION_FULL_ACCESS;
    region.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    region.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    region.IsCacheable = MPU_ACCESS_CACHEABLE;
    region.IsBufferable = MPU_ACCESS_BUFFERABLE;   // with C=1, TEX=0: write-back, no write-allocate
    HAL_MPU_ConfigRegion(&region);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

#ifndef PRISM_RENODE
/**
  * 216 MHz from the 25 MHz HSE, which needs the over-drive mode (ST's F7 examples):
  * PLL VCO = 25 MHz / 25 * 432 = 432 MHz, SYSCLK = 432 / 2, APB1 = 54 MHz, APB2 = 108 MHz.
  * Renode's PWR model has no over-drive handshake, so the emulated build runs at Renode's own
  * clock and skips this.
  */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {};
    RCC_ClkInitTypeDef clk = {};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 25;
    osc.PLL.PLLN = 432;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        for (;;) { __asm volatile ("wfi"); }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
        for (;;) { __asm volatile ("wfi"); }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_7) != HAL_OK)
        for (;;) { __asm volatile ("wfi"); }

    /* The log UART was set up at the reset clock; APB2 is 108 MHz now. */
    boot_log_setup(HAL_RCC_GetPCLK2Freq());
}
#endif

// --- Firmware ---

// The profiling dump's transport: COM1 (USART1, the ST-LINK's virtual COM port). The build is
// profiling-on (meson.build: the prism subproject's profiling=enabled), so the PRISM_PROFILE_*
// marks record into prism::profile::global() while the app runs.
static UartStream uart;

// The user (key) button dumps the recorder over that port, on the press edge (released ->
// pressed). Registered as an on_loop hook, so it runs on the logic thread once per loop turn.
// dump() pauses the recorder for its duration and resumes it on return, so the ~1 s the UART
// takes to drain are simply not recorded (the screen freezes meanwhile).
static void dump_on_button()
{
    static bool was_pressed = false;
    const bool pressed = BSP_PB_GetState(BUTTON_KEY) == GPIO_PIN_SET;
    if (pressed && !was_pressed)
        (void)prism::profile::dump(prism::profile::global(), uart); // no cable: the dump is lost, not an error
    was_pressed = pressed;
}

// The scope's samples: the ADC on Arduino A0, or the generated sweep where it does not start —
// and always in Renode, which does not emulate the ADC.
static prism_examples::SampleSource &scope_source()
{
    static prism_examples::SyntheticSource synthetic{AdcSource::kSampleRate};
#ifdef PRISM_RENODE
    return synthetic;
#else
    static AdcSource adc;
    if (adc.init())
        return adc;
    return synthetic;
#endif
}

// The allocation guard (meson option `alloc_guard`), armed at the first timing line: startup
// is over by then. PRISM's runtime arms it itself only when PRISM is built with the define.
static void arm_allocation_guard([[maybe_unused]] prism::platform::FrameContext &ctx)
{
#ifdef PRISM_ALLOC_GUARD
    ctx.on_stats([](std::string_view) { prism_allocation_guard_arm(); });
#endif
}

// Runs the example this firmware was built with (meson option `example`); returns if it quits.
static void run_example(Display &display, Touch &touch)
{
    using prism::platform::FrameContext;
#ifdef PRISM_EXAMPLE_SCOPE
    static prism_examples::ScopeModel model;
    static prism_examples::ScopeState state{scope_source()};
    prism::platform::frame_app(display, touch, model, [](FrameContext &ctx) {
        prism_examples::attach_scope(ctx, model, state);
        arm_allocation_guard(ctx);
        ctx.on_loop(dump_on_button); /* the key button: dump the profile recorder over COM1 */
    });
#else
    static prism_examples::WidgetsModel model;
    // PRISM's WidgetsModel titles itself "PRISM"; this board's demo says what it runs on.
    model.title.set({.value = "PRISM on STM32F7", .heading = true});
    prism::platform::frame_app(display, touch, model, [](FrameContext &ctx) {
        prism_examples::attach_widgets(ctx, model);
        arm_allocation_guard(ctx);
        ctx.on_loop(dump_on_button); /* the key button: dump the profile recorder over COM1 */
    });
#endif
}

static void app_task(void *)
{
    // Static so they live for the whole app.
    static Display display;
    static Touch touch;
    if (!display.init())
    {
        // No screen: park here; LED1 still blinks from the idle hook.
        for (;;)
        {
            __asm volatile ("wfi");
        }
    }
    // A touch controller that does not answer leaves the app running without input, so the
    // screen still shows that PRISM runs (this board has a single LED, which the idle hook owns).
    (void)touch.init();
    run_example(display, touch);
    // A FreeRTOS task must not return.
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// The whole SDRAM written with a pattern that depends on each word's address, then read back:
// a dead address line, a bad bank or wrong timings show up as the first mismatch, printed. Runs
// before anything uses the SDRAM (framebuffers included), so it may overwrite all of it.
static void sdram_test()
{
    auto *const first = reinterpret_cast<volatile std::uint32_t *>(0xC0000000u);
    constexpr std::uint32_t kWords = 8u * 1024u * 1024u / 4u;
    const auto pattern = [](std::uint32_t i) { return (i * 0x9E3779B1u) ^ 0xA5A5A5A5u; };
    for (std::uint32_t i = 0; i < kWords; ++i)
        first[i] = pattern(i);
    SCB_CleanInvalidateDCache();   // read back from the chip, not from the cache
    std::uint32_t bad = 0;
    for (std::uint32_t i = 0; i < kWords; ++i) {
        const std::uint32_t got = first[i];
        if (got == pattern(i)) continue;
        if (bad == 0) {
            boot_log("f746: sdram FAIL at ");
            boot_log_hex(0xC0000000u + 4u * i);
            boot_log(" wrote ");
            boot_log_hex(pattern(i));
            boot_log(" read ");
            boot_log_hex(got);
            boot_log("\n");
        }
        ++bad;
    }
    if (bad == 0) {
        boot_log("f746: sdram ok (8 MB)\n");
        return;
    }
    boot_log("f746: sdram bad words ");
    boot_log_hex(bad);
    boot_log("\n");
}

extern "C" int main(void)
{
  /* MPU + caches first (ST's order): the MPU must be configured before any SDRAM access, and
     the caches before the C library runs. The CMSIS startup already called SystemInit (FPU on,
     VTOR = flash). */
  /* First thing: the ST-LINK's virtual COM port at 115200 (APB2 = HSI = 16 MHz here), so a
     board that stops has told us where. boot_log_setup() is called again after the clock switch. */
  boot_log_setup(16000000u);
  boot_log("f746: boot\n");

  MPU_Config();
  CPU_CACHE_Enable();
  boot_log("f746: mpu+cache\n");

  /* HAL_Init starts the HAL timebase on TIM6 (FreeRTOS owns SysTick). */
  HAL_Init();
  boot_log("f746: hal\n");

#ifndef PRISM_RENODE
  /* 216 MHz from the 25 MHz HSE (over-drive). */
  SystemClock_Config();
  boot_log("f746: clocks\n");
#endif

  /* The SDRAM holds both heaps, so it starts before anything allocates: the first task's stack
     comes from heap_4's pool there. Not earlier than here: the BSP's sequence waits with
     HAL_Delay, which needs the HAL tick HAL_Init started. Nothing before main allocates (the
     .init_array entries are PRISM's theme constants and libstdc++ hooks). Renode has no FMC. */
#ifndef PRISM_RENODE
  if (BSP_SDRAM_Init() != SDRAM_OK) boot_log("f746: sdram init failed\n");
#endif
  sdram_test();

  BSP_LED_Init(LED1); /* green, blinks from vApplicationIdleHook */

  /* The key button (PI11, polled as a plain GPIO) triggers the profiling dump, and COM1
     (USART1, the ST-LINK's virtual COM port) carries it — both after the clocks, since
     USART1's kernel clock comes from APB2. */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);
  uart.init();

  xTaskCreate(app_task, "app", 8192, nullptr, 2, nullptr); /* 32 KiB stack: frame_app's pthreads come from the heap */
  boot_log("f746: tasks\n");
  vTaskStartScheduler();

  for (;;)
  {
    __asm volatile ("wfi");
  }
}
