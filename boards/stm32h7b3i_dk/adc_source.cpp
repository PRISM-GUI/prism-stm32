#include "adc_source.hpp"

#include "app/adc_sample.hpp"

#include "stm32h7xx_hal.h"

#include <array>
#include <cstdint>

namespace {

// 256 samples (16 ms) per half: one interrupt per half, as often as the old audio path.
constexpr std::size_t kSamplesPerHalf = 256;
// In cacheable AXI SRAM: aligned to and sized in 32-byte cache lines, so invalidating a half
// before reading it touches nothing else.
alignas(32) std::array<std::uint16_t, 2 * kSamplesPerHalf> g_raw;
static_assert(sizeof g_raw % 64 == 0, "each half must be whole cache lines");
std::array<float, kSamplesPerHalf> g_converted; // used from the DMA interrupt only

ADC_HandleTypeDef g_adc{};
DMA_HandleTypeDef g_dma{};
TIM_HandleTypeDef g_tim{};
AdcSource *g_source = nullptr;

void push_half(std::size_t half)
{
    if (g_source == nullptr)
        return;
    std::uint16_t *raw = g_raw.data() + half * kSamplesPerHalf;
    SCB_InvalidateDCache_by_Addr(raw, int(kSamplesPerHalf * sizeof *raw));
    app::adc_to_samples({raw, kSamplesPerHalf}, g_converted);
    g_source->push(g_converted);
}

// The ADC's kernel clock: the HSI (64 MHz) through the CKPER mux, divided by 4 in the ADC.
// SystemClock_Config runs from the HSE, so the HSI is switched on here.
bool start_adc_clock()
{
    RCC_OscInitTypeDef osc{};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        return false;
    RCC_PeriphCLKInitTypeDef clk{};
    clk.PeriphClockSelection = RCC_PERIPHCLK_ADC | RCC_PERIPHCLK_CKPER;
    clk.CkperClockSelection = RCC_CLKPSOURCE_HSI;
    clk.AdcClockSelection = RCC_ADCCLKSOURCE_CLKP;
    if (HAL_RCCEx_PeriphCLKConfig(&clk) != HAL_OK)
        return false;
    __HAL_RCC_ADC12_CLK_ENABLE();
    return true;
}

void configure_pin()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pin{};
    pin.Pin = GPIO_PIN_4;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
}

bool configure_dma()
{
    __HAL_RCC_DMA1_CLK_ENABLE();
    g_dma.Instance = DMA1_Stream0;
    g_dma.Init.Request = DMA_REQUEST_ADC1;
    g_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma.Init.MemInc = DMA_MINC_ENABLE;
    g_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    g_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    g_dma.Init.Mode = DMA_CIRCULAR;
    g_dma.Init.Priority = DMA_PRIORITY_HIGH;
    g_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&g_dma) != HAL_OK)
        return false;
    __HAL_LINKDMA(&g_adc, DMA_Handle, g_dma);
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    return true;
}

bool configure_adc()
{
    g_adc.Instance = ADC1;
    g_adc.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
    g_adc.Init.Resolution = ADC_RESOLUTION_16B;
    g_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    g_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    g_adc.Init.LowPowerAutoWait = DISABLE;
    g_adc.Init.ContinuousConvMode = DISABLE;
    g_adc.Init.NbrOfConversion = 1;
    g_adc.Init.DiscontinuousConvMode = DISABLE;
    g_adc.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_TRGO;
    g_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    g_adc.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DMA_CIRCULAR;
    g_adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    g_adc.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
    g_adc.Init.OversamplingMode = DISABLE;
    if (HAL_ADC_Init(&g_adc) != HAL_OK)
        return false;
    ADC_ChannelConfTypeDef channel{};
    channel.Channel = ADC_CHANNEL_18; // PA4
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    channel.SingleDiff = ADC_SINGLE_ENDED;
    channel.OffsetNumber = ADC_OFFSET_NONE;
    if (HAL_ADC_ConfigChannel(&g_adc, &channel) != HAL_OK)
        return false;
    return HAL_ADCEx_Calibration_Start(&g_adc, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) == HAL_OK;
}

// TIM3 sits on APB1; its clock is twice APB1's whenever APB1 is divided (RM0455, RCC).
std::uint32_t tim3_clock()
{
    const std::uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    const bool apb1_divided = (RCC->CDCFGR2 & (4U << RCC_CDCFGR2_CDPPRE1_Pos)) != 0;
    return apb1_divided ? 2 * pclk1 : pclk1;
}

bool configure_trigger(float sample_rate)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    g_tim.Instance = TIM3;
    g_tim.Init.Prescaler = 0;
    g_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_tim.Init.Period = std::uint32_t(float(tim3_clock()) / sample_rate + 0.5f) - 1;
    g_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    g_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&g_tim) != HAL_OK)
        return false;
    TIM_MasterConfigTypeDef master{};
    master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    return HAL_TIMEx_MasterConfigSynchronization(&g_tim, &master) == HAL_OK;
}

} // namespace

bool AdcSource::init()
{
    if (!start_adc_clock())
        return false;
    configure_pin();
    if (!configure_dma() || !configure_adc() || !configure_trigger(kSampleRate))
        return false;
    g_source = this;
    if (HAL_ADC_Start_DMA(&g_adc, reinterpret_cast<std::uint32_t *>(g_raw.data()), g_raw.size()) != HAL_OK)
        return false;
    return HAL_TIM_Base_Start(&g_tim) == HAL_OK;
}

extern "C" void DMA1_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&g_dma); }

extern "C" void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *)
{
    push_half(0);
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *)
{
    push_half(1);
}
