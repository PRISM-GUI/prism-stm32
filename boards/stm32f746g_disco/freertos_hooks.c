/* FreeRTOS application hooks for the STM32F746G-DISCO firmware. There is no log channel
   (the UART is only set up for profiling dumps), so the fault hooks just park; the idle
   hook blinks the green LED1 so the board shows it is alive even if the screen stays black.
   This board has a single user LED (PI1). */
#include "stm32f7xx_hal.h"
#include "stm32746g_discovery.h"

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
        BSP_LED_Toggle(LED1);
    }
}
