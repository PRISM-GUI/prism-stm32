/**
  * @file    is42s16800j_conf.h
  * @brief   IS42S16800J SDRAM configuration for the STM32H7B3I-DK. From ST's
  *          is42s16800j_conf_template.h, with the refresh count recalculated for this
  *          board: MX_SDRAM_BANK2_Init (stm32h7b3i_discovery_sdram.c) sets
  *          FMC_SDRAM_CLOCK_PERIOD_3, i.e. SDCLK = HCLK/3 = 280/3 = 93.3 MHz, and the
  *          IS42S16800J needs a refresh every 64 ms / 4096 rows = 15.625 us, so
  *          REFRESH_COUNT = 93.3e6 * 15.625e-6 - 20 = 1438 (the template's 0x0603 assumes
  *          a 100 MHz SDRAM clock and would refresh too slowly at 93.3 MHz).
  */
#ifndef IS42S16800J_CONF_H
#define IS42S16800J_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32h7xx_hal.h"

#define REFRESH_COUNT                    ((uint32_t)1438)   /* 93.3 MHz SDCLK, 15.625 us per row; ST's example ships 0x0603 (100 MHz) */
#define IS42S16800J_TIMEOUT             ((uint32_t)0xFFFF)

#ifdef __cplusplus
}
#endif

#endif /* IS42S16800J_CONF_H */
