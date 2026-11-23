#pragma once

// USART1 -- the ST-LINK's virtual COM port (the /dev/ttyACM0 the host sees) -- as a
// prism::io::ByteStream: the transport for prism::profile::dump()
// (doc/design/profiling.md, "Dump"). 921600 baud, 8N1, blocking HAL_UART_Transmit writes: no
// DMA and no interrupts in v1. Dumps only: read_some reports a closed stream (0) and close
// is a no-op, because in v1 the host never sends the board anything.
#include <prism/io/byte_stream.hpp>

#include <cstddef>
#include <expected>
#include <span>
#include <system_error>

class UartStream {
public:
    // COM1 at 921600 8N1 through the BSP's COM support (BSP_COM_Init sets up USART1, its TX PA9 /
    // RX PB7 AF7 pins and the clocks; this BSP expects the caller to fill the handle in). Call
    // once the system clock is up: USART1's kernel clock comes from APB2, so before
    // SystemClock_Config there is no baud to set.
    bool init();

    // ByteStream: all of `out`, blocking (HAL_UART_Transmit, HAL_MAX_DELAY).
    std::expected<void, std::error_code> write(std::span<const std::byte> out);
    // Dumps only in v1: 0, the "closed" of the ByteStream concept.
    std::expected<std::size_t, std::error_code> read_some(std::span<std::byte>);
    void close() {}

private:
    bool ready_ = false; // init failed: write reports an error, the app still runs
};

static_assert(prism::io::ByteStream<UartStream>);
