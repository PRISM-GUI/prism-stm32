/* STM32F746G-DISCO interrupt handlers.
   SysTick/SVC/PendSV belong to FreeRTOS: FreeRTOSConfig.h maps vPortSVCHandler /
   xPortPendSVHandler / xPortSysTickHandler onto those vector names, and the startup
   file's weak aliases give way to the port's definitions. The peripheral interrupts
   this firmware enables are the HAL timebase (TIM6), the ADC's DMA stream (the audio
   scope, defined in adc_capture.cpp) and the LTDC's reload (display.cpp). The touch
   panel is polled over I2C3, not interrupt-driven. */
#include "stm32f7xx_hal.h"

#include "boot_log.h"

/* From boards/stm32f746g_disco/stm32f7xx_hal_timebase_tim.c. */
extern TIM_HandleTypeDef TimHandle;

void TIM6_DAC_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&TimHandle);
}

/* A fault on this board means a blank screen unless something says so: hand the stacked frame
   (MSP or PSP, whichever was in use) to the reporter, which prints the fault registers and the
   frame on the ST-LINK's virtual COM port at 115200. */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
      "tst   lr, #4\n"
      "ite   eq\n"
      "mrseq r0, msp\n"
      "mrsne r0, psp\n"
      "b     boot_log_fault\n");
}
