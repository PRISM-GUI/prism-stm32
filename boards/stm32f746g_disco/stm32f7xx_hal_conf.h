/**
  ******************************************************************************
  * @file    stm32f7xx_hal_conf.h
  * @brief   HAL configuration for the STM32F746G-DISCO firmware. From ST's F7 HAL
  *          template, trimmed to the modules this firmware actually uses: RCC, PWR,
  *          FLASH, CORTEX, GPIO, EXTI, DMA, DMA2D, LTDC, SDRAM (FMC), I2C, TIM, UART.
  *          The HAL timebase is TIM6 (see stm32f7xx_hal_timebase_tim.c) because FreeRTOS
  *          owns SysTick.
  ******************************************************************************
  */
#ifndef STM32F7xx_HAL_CONF_H
#define STM32F7xx_HAL_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_DMA2D_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_LTDC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_SDRAM_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

/* ########################## Oscillator Values adaptation ####################*/
/* The 32F746GDISCOVERY's HSE crystal is 25 MHz (the H7 board's is 24 MHz). */
#if !defined  (HSE_VALUE)
#define HSE_VALUE    ((uint32_t)25000000) /*!< Value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined  (HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    ((uint32_t)100)   /*!< Time out for HSE start up, in ms */
#endif /* HSE_STARTUP_TIMEOUT */

#if !defined  (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)16000000) /*!< Value of the Internal oscillator in Hz*/
#endif /* HSI_VALUE */

#if !defined  (LSE_VALUE)
  #define LSE_VALUE    ((uint32_t)32768) /*!< Value of the External Low Speed oscillator in Hz */
#endif /* LSE_VALUE */

#if !defined  (LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    ((uint32_t)5000)   /*!< Time out for LSE start up, in ms */
#endif /* LSE_STARTUP_TIMEOUT */

#if !defined  (LSI_VALUE)
  #define LSI_VALUE  ((uint32_t)32000)      /*!< Value of the Internal Low Speed oscillator in Hz */
#endif /* LSI_VALUE */

#if !defined  (EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE    12288000U /*!< Value of the External clock in Hz */
#endif /* EXTERNAL_CLOCK_VALUE */

/* ########################### System Configuration ######################### */
#define  VDD_VALUE                    3300UL /*!< Value of VDD in mv */
/* The HAL tick is TIM6 (stm32f7xx_hal_timebase_tim.c). 0x0F is a raw NVIC priority value:
   numerically below configMAX_SYSCALL_INTERRUPT_PRIORITY (0x50), i.e. more urgent than any
   interrupt that may call FreeRTOS FromISR APIs — safe, because the TIM6 handler only
   calls HAL_IncTick(). */
#define  TICK_INT_PRIORITY            ((uint32_t)0x0F)
#define  USE_RTOS                     0
#define  USE_SD_TRANSCEIVER           0U

#define  USE_HAL_ADC_REGISTER_CALLBACKS     0U
#define  USE_HAL_CORTEX_REGISTER_CALLBACKS  0U
#define  USE_HAL_DMA_REGISTER_CALLBACKS     0U
#define  USE_HAL_DMA2D_REGISTER_CALLBACKS   0U
#define  USE_HAL_EXTI_REGISTER_CALLBACKS    0U
#define  USE_HAL_FLASH_REGISTER_CALLBACKS   0U
#define  USE_HAL_I2C_REGISTER_CALLBACKS     0U
#define  USE_HAL_LTDC_REGISTER_CALLBACKS    0U
#define  USE_HAL_PWR_REGISTER_CALLBACKS     0U
#define  USE_HAL_RCC_REGISTER_CALLBACKS     0U
#define  USE_HAL_SDRAM_REGISTER_CALLBACKS   0U
#define  USE_HAL_TIM_REGISTER_CALLBACKS     0U
#define  USE_HAL_UART_REGISTER_CALLBACKS    0U

/* ########################## Assert Selection ############################## */
/* #define USE_FULL_ASSERT    1 */

/* ################## SPI peripheral configuration ########################## */
#define USE_SPI_CRC                   0U

/* Includes ------------------------------------------------------------------*/
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f7xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f7xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32f7xx_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */

#ifdef HAL_DMA2D_MODULE_ENABLED
 #include "stm32f7xx_hal_dma2d.h"
#endif /* HAL_DMA2D_MODULE_ENABLED */

#ifdef HAL_EXTI_MODULE_ENABLED
 #include "stm32f7xx_hal_exti.h"
#endif /* HAL_EXTI_MODULE_ENABLED */

#ifdef HAL_CORTEX_MODULE_ENABLED
 #include "stm32f7xx_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_FLASH_MODULE_ENABLED
 #include "stm32f7xx_hal_flash.h"
#endif /* HAL_FLASH_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
 #include "stm32f7xx_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_LTDC_MODULE_ENABLED
#include "stm32f7xx_hal_ltdc.h"
#endif /* HAL_LTDC_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
 #include "stm32f7xx_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_SDRAM_MODULE_ENABLED
 #include "stm32f7xx_hal_sdram.h"
#endif /* HAL_SDRAM_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
 #include "stm32f7xx_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

/* The audio scope's input: the ADC on Arduino A0 (boards/stm32f746g_disco/adc_source.cpp). */
#ifdef HAL_ADC_MODULE_ENABLED
 #include "stm32f7xx_hal_adc.h"
#endif /* HAL_ADC_MODULE_ENABLED */

/* COM1 (USART1 on PA9/PB7, the ST-LINK's virtual COM port): the profiling dump's transport
   (boards/stm32f746g_disco/uart_stream.cpp). */
#ifdef HAL_UART_MODULE_ENABLED
 #include "stm32f7xx_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif /* STM32F7xx_HAL_CONF_H */
