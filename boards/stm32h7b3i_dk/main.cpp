// STM32H7B3I-DK firmware entry point: board bring-up (MPU, caches, HAL, clocks), the user
// LED, then one FreeRTOS task that runs a PRISM app through PRISM's frame runtime
// (frame_app) — a logic thread and a render thread on top of it. Built twice: stm32h7b3i_dk.elf
// for the real board, and stm32h7b3i_dk_renode.elf (-DPRISM_RENODE) for Renode, which only
// differs in display.cpp.
#include "display.hpp"
#include "touch.hpp"
#include "uart_stream.hpp"
#include "app/widgets_app.hpp"
#include "app/audio_scope.hpp"
#include "app/synthetic_source.hpp"
#include "adc_source.hpp"

#include <prism/platform/alloc_count.hpp>
#include <prism/platform/frame_app.hpp>
#include <prism/profile/dump.hpp>

#include "stm32h7b3i_discovery.h"
#include "stm32h7b3i_discovery_sdram.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

// --- From ST's cube-example main.c for this board, verbatim ---

static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU as Strongly ordered for not defined regions */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x00;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU attributes as WT for SDRAM */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = SDRAM_DEVICE_ADDR;
  MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Configure the MPU OSPI flash */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

/**
  * System Clock Configuration: 280 MHz from the 24 MHz HSE (verbatim from the example).
  * Renode 1.17's RCC model gets through it too (checked 2026-11-20).
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 280;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;

  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    while(1);
  }

  /* Select PLL as system clock source and configure  bus clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
    RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6);
  if(ret != HAL_OK)
  {
     while(1);
  }

  __HAL_RCC_CSI_ENABLE() ;

  __HAL_RCC_SYSCFG_CLK_ENABLE() ;

  HAL_EnableCompensationCell();
}

// --- Firmware ---

// The profiling dump's transport: COM1 (USART1 on PA9/PA10, the ST-LINK's virtual COM port).
// The build is profiling-on (meson.build: the prism subproject's profiling=enabled), so the
// PRISM_PROFILE_* marks record into prism::profile::global() while the app runs.
static UartStream uart;

// The blue user button dumps the recorder over that port, on the press edge (released ->
// pressed). Registered as an on_loop hook, so it runs on the logic thread once per loop turn.
// dump() pauses the recorder for its duration and resumes it on return, so the ~2 s the UART
// takes to drain 1024 events x 6 threads are simply not recorded (the screen freezes meanwhile).
static void dump_on_button()
{
    static bool was_pressed = false;
    const bool pressed = BSP_PB_GetState(BUTTON_USER) == BUTTON_PRESSED;
    if (pressed && !was_pressed)
        (void)prism::profile::dump(prism::profile::global(), uart); // no cable: the dump is lost, not an error
    was_pressed = pressed;
}

// The scope's samples: the ADC on Arduino A0, or the generated sweep where it does not start —
// and always in Renode, which does not emulate the ADC.
static app::SampleSource &scope_source()
{
    static app::SyntheticSource synthetic{AdcSource::kSampleRate};
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
    static app::ScopeModel model;
    static app::ScopeState state{scope_source()};
    prism::platform::frame_app(display, touch, model, [](FrameContext &ctx) {
        app::attach_scope(ctx, model, state);
        arm_allocation_guard(ctx);
        ctx.on_loop(dump_on_button); /* the blue button: dump the profile recorder over COM1 */
    });
#else
    static app::WidgetsModel model;
    prism::platform::frame_app(display, touch, model, [](FrameContext &ctx) {
        app::attach_widgets(ctx, model);
        arm_allocation_guard(ctx);
        ctx.on_loop(dump_on_button); /* the blue button: dump the profile recorder over COM1 */
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
        // No screen: park here; LED2 still blinks from the idle hook.
        for (;;)
        {
            __asm volatile ("wfi");
        }
    }
    // A touch controller that does not answer leaves the app running without input, so the
    // screen still shows that PRISM runs; LED3 lit says touch is missing.
    if (!touch.init())
        BSP_LED_On(LED3);
    run_example(display, touch);
    // A FreeRTOS task must not return.
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

extern "C" int main(void)
{
  /* MPU + caches first (the example's order): the MPU must be configured before any
     AXI/SDRAM access, and the caches before the C library runs. The CMSIS startup already
     called ExitRun0Mode (power supply = direct SMPS, see USE_PWR_DIRECT_SMPS_SUPPLY) and
     SystemInit (FPU on, VTOR = flash). */
  MPU_Config();
  CPU_CACHE_Enable();

  /* HAL_Init starts the HAL timebase on TIM6 (FreeRTOS owns SysTick). */
  HAL_Init();

  /* 280 MHz from the 24 MHz HSE. */
  SystemClock_Config();

  BSP_LED_Init(LED2); /* blinks from vApplicationIdleHook */
  BSP_LED_Init(LED3); /* red: lit when the touch controller does not answer */

  /* The blue user button (PC13, polled as a plain GPIO) triggers the profiling dump, and COM1
     (USART1 on PA9/PA10, the ST-LINK's virtual COM port) carries it — both after the clocks,
     since USART1's kernel clock comes from APB2. */
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);
  uart.init();

  xTaskCreate(app_task, "app", 8192, nullptr, 2, nullptr); /* 32 KiB stack: frame_app's pthreads come from the heap */
  vTaskStartScheduler();

  for (;;)
  {
    __asm volatile ("wfi");
  }
}
