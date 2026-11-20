/* FreeRTOS application hooks for the STM32H7B3I-DK firmware. There is no log channel
   (no UART is set up), so the fault hooks just park; the idle hook blinks LED2 so the
   board shows it is alive even if the screen stays black. */
#include "stm32h7xx_hal.h"
#include "stm32h7b3i_discovery.h"

#include "FreeRTOS.h"
#include "task.h"

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
        BSP_LED_Toggle(LED2);
    }
}
