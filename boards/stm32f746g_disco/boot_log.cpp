// Early progress log and fault report on USART1 (the ST-LINK's virtual COM port, PA9 TX / PB7 RX).
// Registers only: this has to work before HAL_Init, before SystemClock_Config and before the
// FreeRTOS scheduler, i.e. from the very first line of main.
#include "boot_log.h"

#include "stm32f7xx.h"

#include "FreeRTOS.h"
#include "task.h"

namespace {

USART_TypeDef *const kUart = USART1;

void put_char(char c)
{
    while ((kUart->ISR & USART_ISR_TXE) == 0)
        ;
    kUart->TDR = static_cast<uint32_t>(static_cast<unsigned char>(c));
}

} // namespace

void boot_log_setup(uint32_t pclk2_hz)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    GPIOA->MODER = (GPIOA->MODER & ~(3u << 18)) | (2u << 18);    // PA9: alternate function
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFu << 4)) | (7u << 4);  // PA9: AF7 = USART1_TX
    GPIOB->MODER = (GPIOB->MODER & ~(3u << 14)) | (2u << 14);    // PB7: alternate function
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(0xFu << 28)) | (7u << 28); // PB7: AF7 = USART1_RX

    kUart->CR1 = 0;                                  // 8 data bits, 1 stop bit, no parity
    kUart->BRR = (pclk2_hz + 57600u) / 115200u;      // USARTDIV in 12.4 fixed point, rounded
    kUart->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void boot_log(const char *text)
{
    for (; *text != '\0'; ++text)
        put_char(*text);
}

void boot_log_hex(uint32_t value)
{
    static const char digits[] = "0123456789abcdef";
    boot_log("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        put_char(digits[(value >> shift) & 0xFu]);
}

void boot_log_fault(uint32_t *frame)
{
    boot_log("\n!! fault");
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        boot_log(" in task ");
        boot_log(pcTaskGetName(nullptr));
    }
    boot_log("\ncfsr ");
    boot_log_hex(*(volatile uint32_t *)0xE000ED28u);
    boot_log(" hfsr ");
    boot_log_hex(*(volatile uint32_t *)0xE000ED2Cu);
    boot_log(" mmfar ");
    boot_log_hex(*(volatile uint32_t *)0xE000ED34u);
    boot_log(" bfar ");
    boot_log_hex(*(volatile uint32_t *)0xE000ED38u);
    boot_log("\npc ");
    boot_log_hex(frame[6]);
    boot_log(" lr ");
    boot_log_hex(frame[5]);
    boot_log(" psr ");
    boot_log_hex(frame[7]);
    boot_log(" r0 ");
    boot_log_hex(frame[0]);
    boot_log(" r1 ");
    boot_log_hex(frame[1]);
    boot_log(" r2 ");
    boot_log_hex(frame[2]);
    boot_log(" r3 ");
    boot_log_hex(frame[3]);
    boot_log("\n");
    for (;;)
        __asm volatile("wfi");
}
