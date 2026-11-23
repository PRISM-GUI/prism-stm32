#pragma once

// Configuration for the MT48LC4M32B2 SDRAM mounted on the STM32F746G-DISCO (1 Mbit x 32 x 4 banks,
// see third_party/stm32-mt48lc4m32b2/mt48lc4m32b2_conf_template.h). Included by mt48lc4m32b2.h.

#include "stm32f7xx_hal.h"

// 4096 refresh cycles per 64 ms, counted in SDCLK periods: 64 ms / 4096 * 108 MHz - 20, where
// 108 MHz is the SDCLK the BSP's FMC setup asks for (HCLK 216 MHz / FMC_SDRAM_CLOCK_PERIOD_2).
#define REFRESH_COUNT ((uint32_t)1667)

#define MT48LC4M32B2_TIMEOUT ((uint32_t)0xFFFF)
