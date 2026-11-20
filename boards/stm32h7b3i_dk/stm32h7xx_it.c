/* STM32H7B3I-DK interrupt handlers, trimmed from ST's cube-example stm32h7xx_it.c.
   SysTick/SVC/PendSV belong to FreeRTOS: FreeRTOSConfig.h maps vPortSVCHandler /
   xPortPendSVHandler / xPortSysTickHandler onto those vector names, and the startup
   file's weak aliases give way to the port's definitions. The peripheral interrupts
   this firmware enables are the HAL timebase, TIM6, and the ADC's DMA stream (the audio
   scope, defined in adc_source.cpp). (LTDC's IRQ is not used: present() polls the reload flag;
   the touch panel is polled, not interrupt-driven; I2C4 is polled.) */
#include "stm32h7xx_hal.h"

/* From boards/stm32h7b3i_dk/stm32h7xx_hal_timebase_tim.c. */
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
