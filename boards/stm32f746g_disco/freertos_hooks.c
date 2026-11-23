/* FreeRTOS application hooks for the STM32F746G-DISCO firmware. There is no log channel
   (the UART is only set up for profiling dumps), so the fault hooks just park; the idle
   hook blinks the green LED1 so the board shows it is alive even if the screen stays black.
   This board has a single user LED (PI1). */
#include "stm32f7xx_hal.h"
#include "stm32746g_discovery.h"

#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS's own memory pool (heap_4). Every task stack comes from it, and PRISM's frame
   runtime asks for a 64 KiB logic thread and a 48 KiB render thread, which does not fit in the
   256 KiB of SRAM next to PRISM's statics. configAPPLICATION_ALLOCATED_HEAP moves the array
   here, and the linker script puts it in the SDRAM above the framebuffers (.sdram_heap) — the
   SDRAM is mapped cacheable write-through by main.cpp's MPU_Config, and no task stack is
   touched by DMA. */
#if ( configAPPLICATION_ALLOCATED_HEAP == 1 )
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".sdram_heap"), aligned(8)));
#endif

void vApplicationMallocFailedHook(void)
{
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

void board_assert_fail(const char *file, int line)
{
    (void)file;
    (void)line;
    for (;;)
    {
        __asm volatile ("wfi");
    }
}

void vApplicationIdleHook(void)
{
    static uint32_t last = 0;
    uint32_t now = HAL_GetTick(); /* TIM6 timebase, started by HAL_Init */
    if (now - last >= 250)
    {
        last = now;
        BSP_LED_Toggle(LED1);
    }
}
