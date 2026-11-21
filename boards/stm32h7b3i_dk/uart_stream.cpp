// The ST-LINK's serial port as a PRISM byte stream (see uart_stream.hpp).
#include "uart_stream.hpp"

#include "stm32h7b3i_discovery.h" // the BSP's COM1: USART1 on PA9/PA10, hcom_uart
#include "stm32h7xx_hal.h"

#include <algorithm>
#include <cstdint>
#include <system_error>

namespace {

// HAL_UART_Transmit's size is a u16: send in pieces if a write is ever longer.
constexpr std::size_t kMaxWrite = 0xFFFF;

} // namespace

bool UartStream::init()
{
    COM_InitTypeDef com{};
    com.BaudRate = 921600;
    com.WordLength = COM_WORDLENGTH_8B;
    com.StopBits = COM_STOPBITS_1;
    com.Parity = COM_PARITY_NONE;
    com.HwFlowCtl = COM_HWCONTROL_NONE;
    ready_ = BSP_COM_Init(COM1, &com) == BSP_ERROR_NONE;
    return ready_;
}

std::expected<void, std::error_code> UartStream::write(std::span<const std::byte> out)
{
    if (!ready_)
        return std::unexpected(std::make_error_code(std::errc::not_connected));
    while (!out.empty()) {
        const std::size_t n = std::min(out.size(), kMaxWrite);
        const HAL_StatusTypeDef status =
            HAL_UART_Transmit(&hcom_uart[COM1], reinterpret_cast<const std::uint8_t*>(out.data()),
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
