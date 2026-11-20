/* HAL timebase on TIM6 for the STM32H7B3I-DK firmware, adapted from ST's
   stm32h7xx_hal_timebase_tim_template.c: FreeRTOS owns SysTick, so the HAL tick
   (HAL_GetTick/HAL_Delay, and the timeouts the BSP's polled I2C uses) comes from TIM6.
   Differences from the template: the handle is not static (stm32h7xx_it.c's
   TIM6_DAC_IRQHandler calls HAL_TIM_IRQHandler on it), and the IRQ handler itself
   lives in stm32h7xx_it.c.

   TIM6 is on APB1; at 280 MHz PCLK1 = 140 MHz and the APB1 prescaler is 2, so the
   timer clock is 280 MHz and the prescaler below gives a 1 MHz counter. HAL computes
   all of that itself from the clock config, so nothing is hardcoded here. */
#include "stm32h7xx_hal.h"

TIM_HandleTypeDef TimHandle;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef    clkconfig;
  uint32_t              uwTimclock, uwAPB1Prescaler;
  uint32_t              uwPrescalerValue;
  uint32_t              pFLatency;
  HAL_StatusTypeDef     status;

  /* Enable TIM6 clock */
  __HAL_RCC_TIM6_CLK_ENABLE();

  /* Get clock configuration */
  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

  /* Get APB1 prescaler */
  uwAPB1Prescaler = clkconfig.APB1CLKDivider;

  /* Compute TIM6 clock */
  if (uwAPB1Prescaler == RCC_HCLK_DIV1)
  {
    uwTimclock = HAL_RCC_GetPCLK1Freq();
  }
  else
  {
    uwTimclock = 2UL * HAL_RCC_GetPCLK1Freq();
  }

  /* Compute the prescaler value to have TIM6 counter clock equal to 1MHz */
  uwPrescalerValue = (uint32_t)((uwTimclock / 1000000U) - 1U);

  /* Initialize TIM6 */
  TimHandle.Instance = TIM6;

  /* Period = [(TIM6CLK/1000) - 1] for a 1 ms time base, prescaler for a 1 MHz counter */
  TimHandle.Init.Period        = (1000000U / 1000U) - 1U;
  TimHandle.Init.Prescaler     = uwPrescalerValue;
  TimHandle.Init.ClockDivision = 0U;
  TimHandle.Init.CounterMode   = TIM_COUNTERMODE_UP;
  status = HAL_TIM_Base_Init(&TimHandle);
  if (status == HAL_OK)
  {
    /* Start the TIM time base generation in interrupt mode */
    status = HAL_TIM_Base_Start_IT(&TimHandle);
    if (status == HAL_OK)
    {
      /* Enable the TIM6 global interrupt */
      HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);

      if (TickPriority < (1UL << __NVIC_PRIO_BITS))
      {
        HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0);
        uwTickPrio = TickPriority;
      }
      else
      {
        status = HAL_ERROR;
      }
    }
  }

  return status;
}

void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&TimHandle, TIM_IT_UPDATE);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  UNUSED(htim);
  HAL_IncTick();
}
