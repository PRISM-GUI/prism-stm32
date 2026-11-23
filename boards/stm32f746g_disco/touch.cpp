// Touch polling for the STM32F746G-DISCO (see touch.hpp).
#include "touch.hpp"

#include "display.hpp"   // Display::kWidth/kHeight: the panel is the LCD's 480x272
#include "ft5336.h"
#include "stm32746g_discovery.h"
#include "stm32f7xx_hal.h"

#include <chrono>
#include <cstdint>
#include <thread>

namespace {

constexpr std::uint32_t kI2cTimeoutMs = 100;
constexpr int kProbeAttempts = 20;   // x 100 ms: the controller answers late after power-up

I2C_HandleTypeDef g_i2c3;
FT5336_Object_t g_touch;

// --- FT5336's bus callbacks (third_party/stm32-ft5336/ft5336.h) ---

int32_t io_init(void)
{
    return FT5336_OK;   // I2C3 is up before the object is registered
}

int32_t io_deinit(void)
{
    return FT5336_OK;
}

int32_t io_write_reg(std::uint16_t address, std::uint16_t reg, std::uint8_t *data, std::uint16_t len)
{
    return HAL_I2C_Mem_Write(&g_i2c3, address, reg, I2C_MEMADD_SIZE_8BIT, data, len, kI2cTimeoutMs)
                   == HAL_OK
               ? FT5336_OK
               : FT5336_ERROR;
}

int32_t io_read_reg(std::uint16_t address, std::uint16_t reg, std::uint8_t *data, std::uint16_t len)
{
    return HAL_I2C_Mem_Read(&g_i2c3, address, reg, I2C_MEMADD_SIZE_8BIT, data, len, kI2cTimeoutMs)
                   == HAL_OK
               ? FT5336_OK
               : FT5336_ERROR;
}

int32_t io_get_tick(void)
{
    return static_cast<int32_t>(HAL_GetTick());
}

// I2C3 on PH7/PH8, at the timing ST ships for this board (DISCOVERY_I2Cx_TIMING, 100 kHz-ish at
// 200 MHz APB1 — the panel's controller does not care that ours is 54 MHz).
bool init_i2c()
{
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_I2C3_CLK_ENABLE();

    GPIO_InitTypeDef pin{};
    pin.Pin = DISCOVERY_AUDIO_I2Cx_SCL_PIN | DISCOVERY_AUDIO_I2Cx_SDA_PIN;
    pin.Mode = GPIO_MODE_AF_OD;
    pin.Pull = GPIO_PULLUP;
    pin.Speed = GPIO_SPEED_FAST;
    pin.Alternate = DISCOVERY_AUDIO_I2Cx_SCL_SDA_AF;
    HAL_GPIO_Init(DISCOVERY_AUDIO_I2Cx_SCL_SDA_GPIO_PORT, &pin);

    g_i2c3.Instance = DISCOVERY_AUDIO_I2Cx;
    g_i2c3.Init.Timing = DISCOVERY_I2Cx_TIMING;
    g_i2c3.Init.OwnAddress1 = 0;
    g_i2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    g_i2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    g_i2c3.Init.OwnAddress2 = 0;
    g_i2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    g_i2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    g_i2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    return HAL_I2C_Init(&g_i2c3) == HAL_OK;
}

bool probe()
{
    if (FT5336_Init(&g_touch) != FT5336_OK)
        return false;
    std::uint32_t id = 0;
    return FT5336_ReadID(&g_touch, &id) == FT5336_OK && id == FT5336_ID;
}

} // namespace

bool Touch::init()
{
    if (!init_i2c())
        return false;
    FT5336_IO_t io{};
    io.Init = io_init;
    io.DeInit = io_deinit;
    io.Address = TS_I2C_ADDRESS;   // 0x70 (8-bit) = 7-bit 0x38
    io.WriteReg = io_write_reg;
    io.ReadReg = io_read_reg;
    io.GetTick = io_get_tick;
    if (FT5336_RegisterBusIO(&g_touch, &io) != FT5336_OK)
        return false;
    for (int attempt = 0; attempt < kProbeAttempts; ++attempt) {
        if (probe()) {
            ready_ = true;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

std::optional<prism::core::Point> Touch::read()
{
    if (!ready_)
        return std::nullopt;
    FT5336_State_t state = {};
    if (FT5336_GetState(&g_touch, &state) != FT5336_OK || state.TouchDetected == 0U)
        return std::nullopt;
    // The panel is mounted rotated: the controller reports the axes swapped (ST's BSP applies
    // TS_SWAP_XY for this FT5336, i.e. x = raw y, y = raw x). Both are already in panel pixels.
    return prism::core::Point{prism::core::X{static_cast<float>(state.TouchY)},
                              prism::core::Y{static_cast<float>(state.TouchX)}};
}
