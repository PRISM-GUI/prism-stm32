#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Progress on the ST-LINK virtual COM port (USART1, PA9/PB7) at 115200. Written against the
   registers directly, so it works from the first line of main: before HAL_Init, before the
   clocks are switched and before FreeRTOS exists. A board that stops after a line has told us
   where it stopped even when the screen stays blank. */
void boot_log_setup(uint32_t pclk2_hz);
void boot_log(const char *text);
void boot_log_hex(uint32_t value);

/* HardFault reporter (boards/stm32f746g_disco/stm32f7xx_it.c): prints the fault registers and the
   stacked frame, then parks. Never returns. */
void boot_log_fault(uint32_t *frame);

#ifdef __cplusplus
}
#endif
