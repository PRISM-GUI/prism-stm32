#include "adc_source.hpp"

#include "stm32f7xx_hal.h"

#include <array>
#include <cstdint>

namespace {

// 256 samples (16 ms) per half: one interrupt per half, as often as the H7 board's audio path.
constexpr std::size_t kSamplesPerHalf = 256;
// In SRAM1 (cacheable by default on this board): aligned to and sized in 32-byte cache lines, so
// invalidating a half before reading it touches nothing else.
alignas(32) std::array<std::uint16_t, 2 * kSamplesPerHalf> g_raw;
static_assert(sizeof g_raw % 64 == 0, "each half must be whole cache lines");
std::array<float, kSamplesPerHalf> g_converted; // used from the DMA interrupt only

ADC_HandleTypeDef g_adc{};
DMA_HandleTypeDef g_dma{};
TIM_HandleTypeDef g_tim{};
AdcSource *g_source = nullptr;

// 12-bit conversion to [-1, 1): mid-scale is 0, one LSB is 1/2048.
float adc_to_sample(std::uint16_t raw)
{
    return (static_cast<float>(raw) - 2048.f) / 2048.f;
}

void push_half(std::size_t half)
{
    if (g_source == nullptr)
        return;
    std::uint16_t *raw = g_raw.data() + half * kSamplesPerHalf;
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<std::uint32_t *>(raw),
                                 static_cast<std::int32_t>(kSamplesPerHalf * sizeof *raw));
    for (std::size_t i = 0; i < kSamplesPerHalf; ++i)
        g_converted[i] = adc_to_sample(raw[i]);
    g_source->push(g_converted);
}

void configure_pin()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef pin{};
    pin.Pin = GPIO_PIN_0;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &pin);
}

// ADC3's request is DMA2 stream 0, channel 2 (RM0385, DMA2 request mapping).
bool configure_dma()
{
    __HAL_RCC_DMA2_CLK_ENABLE();
    g_dma.Instance = DMA2_Stream0;
    g_dma.Init.Channel = DMA_CHANNEL_2;
    g_dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    g_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    g_dma.Init.MemInc = DMA_MINC_ENABLE;
    g_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    g_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    g_dma.Init.Mode = DMA_CIRCULAR;
    g_dma.Init.Priority = DMA_PRIORITY_HIGH;
    g_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    g_dma.Init.MemBurst = DMA_MBURST_SINGLE;
    g_dma.Init.PeriphBurst = DMA_PBURST_SINGLE;
    if (HAL_DMA_Init(&g_dma) != HAL_OK)
        return false;
    __HAL_LINKDMA(&g_adc, DMA_Handle, g_dma);
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    return true;
}

// 12 bits, right-aligned, one channel per trigger; the clock is APB2's (108 MHz) divided by 4,
// the ADC's maximum being 36 MHz. This ADC has no software calibration (the F7 HAL has none).
bool configure_adc()
{
    __HAL_RCC_ADC3_CLK_ENABLE();
    g_adc.Instance = ADC3;
    g_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    g_adc.Init.Resolution = ADC_RESOLUTION_12B;
    g_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_adc.Init.ScanConvMode = DISABLE;
    g_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    g_adc.Init.ContinuousConvMode = DISABLE;
    g_adc.Init.NbrOfConversion = 1;
    g_adc.Init.DiscontinuousConvMode = DISABLE;
    g_adc.Init.NbrOfDiscConversion = 1;
    g_adc.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
    g_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    g_adc.Init.DMAContinuousRequests = ENABLE;
    if (HAL_ADC_Init(&g_adc) != HAL_OK)
        return false;
    ADC_ChannelConfTypeDef channel{};
    channel.Channel = ADC_CHANNEL_0; // PA0, Arduino A0
    channel.Rank = 1;
    channel.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    channel.Offset = 0;
    return HAL_ADC_ConfigChannel(&g_adc, &channel) == HAL_OK;
}

// TIM2 sits on APB1; its clock is twice APB1's whenever APB1 is divided (RM0385, RCC).
std::uint32_t tim2_clock()
{
    const std::uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    const bool apb1_divided = (RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1;
    return apb1_divided ? 2 * pclk1 : pclk1;
}

bool configure_trigger(float sample_rate)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    g_tim.Instance = TIM2;
    g_tim.Init.Prescaler = 0;
    g_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    g_tim.Init.Period = std::uint32_t(float(tim2_clock()) / sample_rate + 0.5f) - 1;
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
    configure_pin();
    if (!configure_dma() || !configure_adc() || !configure_trigger(kSampleRate))
        return false;
    g_source = this;
    if (HAL_ADC_Start_DMA(&g_adc, reinterpret_cast<std::uint32_t *>(g_raw.data()), g_raw.size()) != HAL_OK)
        return false;
    return HAL_TIM_Base_Start(&g_tim) == HAL_OK;
}

extern "C" void DMA2_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&g_dma); }

extern "C" void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *)
{
    push_half(0);
}

extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *)
{
    push_half(1);
}
