// STM32H7B3I-DK port: board bring-up (MPU, caches, HAL, clocks), the user LED, then one FreeRTOS
// task that registers this board as PRISM's port — the panel and the touch as the port's frame
// sink and input, the ADC on A0 as a capture device — and runs PRISM's own example main() on it
// (meson option `example`: examples/embedded/{widgets,scope}_main.cpp, unchanged). Built twice:
// stm32h7b3i_dk.elf for the real board, and stm32h7b3i_dk_renode.elf (-DPRISM_RENODE) for Renode,
// which only differs in display.cpp and the ADC.
#include "adc_capture.hpp"
#include "display.hpp"
#include "touch.hpp"
#include "uart_stream.hpp"

#include <prism/platform/alloc_count.hpp>
#include <prism/platform/frame_app.hpp>
#include <prism/platform/port.hpp>
#include <prism/profile/dump.hpp>

#include "stm32h7b3i_discovery.h"
#include "stm32h7b3i_discovery_sdram.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstddef>
#include <functional>
#include <span>
#include <string_view>
#include <utility>

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

// The profiling dump's transport: COM1 (USART1 on PA9/PA10, the ST-LINK's virtual COM port), and
// the timing line's, below. The build is profiling-on (meson.build: the prism subproject's
// profiling=enabled), so the PRISM_PROFILE_* marks record into prism::profile::global() while the
// app runs.
static UartStream uart;

// For the whole run: the port's run_frames closes over both, and PRISM's runtime draws on the
// panel from its own thread until the app's main() returns.
static Display display;
static Touch touch;

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

// The port's timing line, over COM1. It runs on the app's thread once a second, so the first line
// is also the moment the allocation guard (meson option `alloc_guard`) goes on: startup is over
// by then.
static void board_stats(std::string_view line)
{
#ifdef PRISM_ALLOC_GUARD
    prism_allocation_guard_arm();
#endif
    (void)uart.write(std::as_bytes(std::span{line.data(), line.size()}));
    (void)uart.write(std::as_bytes(std::span{"\r\n", 2}));
}

// What platform::run calls: PRISM's frame runtime over this board's panel and touch, with the
// default FrameOptions (no before_thread_start, UI scale 1) as this board always ran, plus the one
// hook an app does not have -- the profile dump when the blue button is pressed.
static void run_board_frames(prism::platform::TreeFactory make_tree,
                             std::function<void(prism::platform::FrameContext &)> setup)
{
    prism::platform::run_frames(
        display, touch, std::move(make_tree),
        [setup = std::move(setup)](prism::platform::FrameContext &fc) {
            // The AppContext and the app's setup, then our hook: both must be registered before
            // setup returns.
            setup(fc);
            fc.on_loop(dump_on_button);
        });
}

// entry.c reaches this through -Wl,--wrap=main (the CMSIS startup's `bl main`), and the app's
// main() through __real_main once the port is registered.
extern "C" int board_app_main(void);

static void app_task(void *)
{
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
#ifndef PRISM_RENODE
    // A0 as PRISM's capture device: the scope app opens it by name. Not registered when the ADC
    // does not start, and the app then shows its synthetic sweep by itself. Renode has no ADC.
    (void)register_adc_capture();
#endif
    prism::platform::set_port({
        .run_frames = run_board_frames,
        .on_stats = board_stats,
    });
    // The app's own main(), on this task and its 32 KiB stack, as it always ran here.
    (void)board_app_main();
    // A FreeRTOS task must not return.
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

// The board's C entry: entry.c's __wrap_main calls it (see meson.build's -Wl,--wrap=main).
extern "C" void board_main(void)
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

  xTaskCreate(app_task, "app", 8192, nullptr, 2, nullptr); /* 32 KiB stack: the runtime's pthreads come from the heap */
  vTaskStartScheduler();

  for (;;)
  {
    __asm volatile ("wfi");
  }
}
