/**
  ******************************************************************************
  * @file    stm32h7b3i_discovery_conf.h
  * @brief   STM32H7B3I-DK board configuration for this firmware. From ST's cube-example for
  *          this board, trimmed to what the LCD/touch/SDRAM BSP paths we compile need.
  *          The two LTDC framebuffers are 480*272*4 = 522240 bytes each; the front buffer is
  *          SDRAM bank 2's base (0xD0000000, where BSP_LCD_InitEx puts layer 0), the back
  *          buffer right after it at 0xD0080000 (see display.cpp).
  ******************************************************************************
  */
#ifndef STM32H7B3I_DISCOVERY_CONF_H
#define STM32H7B3I_DISCOVERY_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* COM define. USE_BSP_COM_FEATURE brings in only the COM1 init/deinit (USART1 on PA9/PA10,
   AF7, their clocks) and hcom_uart: the profiling dump's transport (uart_stream.cpp). The
   log port stays off: printf goes to semihosting, not to the ST-LINK. */
#define USE_COM_LOG                         0U
#define USE_BSP_COM_FEATURE                 1U

/* I2C BUS timing define */
#define I2C_VALID_TIMING_NBR                128U

/* LCD defines */
#define LCD_LAYER_0_ADDRESS                 0xD0000000U   /* front buffer, layer 0 */
#define LCD_LAYER_1_ADDRESS                 0xD0080000U   /* our back buffer (unused as layer 1) */
#define USE_DMA2D_TO_FILL_RGB_RECT          0U
#define USE_LCD_CTRL_RK043FN48H             1U

/* Camera sensor define */
#define USE_CAMERA_SENSOR_OV9655            0U

/* Audio codecs defines */
#define USE_AUDIO_CODEC_CS42L51             0U
#define USE_BSP_CPU_CACHE_MAINTENANCE       1U

/* TS defines: no gesture recognition; multi-touch state is still read (single touch used) */
#define USE_TS_GESTURE                      0U
#define USE_TS_MULTI_TOUCH                  1U
#define TS_TOUCH_NBR                        2U

/* Default EEPROM max trials */
#define EEPROM_MAX_TRIALS                   3000U

/* IRQ priorities (raw NVIC values; only the ones we could enable) */
#define BSP_SDRAM_IT_PRIORITY               15U
#define BSP_CAMERA_IT_PRIORITY              15U
#define BSP_BUTTON_USER_IT_PRIORITY         15U
#define BSP_AUDIO_OUT_IT_PRIORITY           14U
#define BSP_AUDIO_IN_IT_PRIORITY            15U
#define BSP_SD_IT_PRIORITY                  14U
#define BSP_SD_RX_IT_PRIORITY               14U
#define BSP_SD_TX_IT_PRIORITY               15U
#define BSP_TS_IT_PRIORITY                  15U

#ifdef __cplusplus
}
#endif

#endif /* STM32H7B3I_DISCOVERY_CONF_H */
