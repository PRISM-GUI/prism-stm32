/* STM32F746G-DISCO interrupt handlers.
   SysTick/SVC/PendSV belong to FreeRTOS: FreeRTOSConfig.h maps vPortSVCHandler /
   xPortPendSVHandler / xPortSysTickHandler onto those vector names, and the startup
   file's weak aliases give way to the port's definitions. The peripheral interrupts
   this firmware enables are the HAL timebase (TIM6), the ADC's DMA stream (the audio
   scope, defined in adc_source.cpp) and the LTDC's reload (display.cpp). The touch
   panel is polled over I2C3, not interrupt-driven. */
#include "stm32f7xx_hal.h"

/* From boards/stm32f746g_disco/stm32f7xx_hal_timebase_tim.c. */
extern TIM_HandleTypeDef TimHandle;

void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&TimHandle);
}

void HardFault_Handler(void)
{
  /* Park: attach a debugger to inspect the stacked frame. */
  for (;;)
  {
    __asm volatile ("wfi");
  }
}
