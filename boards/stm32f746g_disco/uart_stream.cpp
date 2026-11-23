// The ST-LINK's serial port as a PRISM byte stream (see uart_stream.hpp).
#include "uart_stream.hpp"

#include "stm32746g_discovery.h" // COM1: USART1 on PA9 (TX) / PB7 (RX), AF7
#include "stm32f7xx_hal.h"

#include <algorithm>
#include <cstdint>
#include <system_error>

namespace {

// HAL_UART_Transmit's size is a u16: send in pieces if a write is ever longer.
constexpr std::size_t kMaxWrite = 0xFFFF;

// This BSP's BSP_COM_Init takes the handle from the caller (the H7's fills its own array).
UART_HandleTypeDef g_uart;

} // namespace

bool UartStream::init()
{
    g_uart.Init.BaudRate = 115200;
    g_uart.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart.Init.StopBits = UART_STOPBITS_1;
    g_uart.Init.Parity = UART_PARITY_NONE;
    g_uart.Init.Mode = UART_MODE_TX_RX;
    g_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    BSP_COM_Init(COM1, &g_uart);   // returns void here: it enables the clocks and pins, then inits
    ready_ = g_uart.gState == HAL_UART_STATE_READY;
    return ready_;
}

std::expected<void, std::error_code> UartStream::write(std::span<const std::byte> out)
{
    if (!ready_)
        return std::unexpected(std::make_error_code(std::errc::not_connected));
    while (!out.empty()) {
        const std::size_t n = std::min(out.size(), kMaxWrite);
        const HAL_StatusTypeDef status =
            HAL_UART_Transmit(&g_uart, reinterpret_cast<const std::uint8_t*>(out.data()),
                              static_cast<std::uint16_t>(n), HAL_MAX_DELAY);
        if (status != HAL_OK)
            return std::unexpected(std::make_error_code(std::errc::io_error));
        out = out.subspan(n);
    }
    return {};
}

std::expected<std::size_t, std::error_code> UartStream::read_some(std::span<std::byte>)
{
    return std::size_t{0}; // dumps only in v1: the stream reads as closed
}
