// The 8 MiB SDRAM has to be up before anything allocates from it: PRISM's C++ heap (newlib's
// _sbrk window) and FreeRTOS's heap_4 pool both live there (stm32f746g.ld), and C++
// static constructors run before main.
//
// `.preinit_array` is the first thing __libc_init_array calls, after the startup code has copied
// .data and zeroed .bss and before any static constructor: the earliest point at which the BSP's
// own SDRAM bring-up can run with initialized RAM (ST's SystemInit hook cannot — .bss is still
// zeroed after SystemInit, which would wipe the HAL handles the BSP's sequence uses).
#include "stm32746g_discovery_sdram.h"

#ifndef PRISM_RENODE   // Renode maps the SDRAM as plain memory and models no FMC

namespace {

void sdram_preinit()
{
    (void)BSP_SDRAM_Init();
}

// The entry itself is a pointer (a function attribute would put the code in the array section).
__attribute__((section(".preinit_array"), used)) void (*const sdram_preinit_entry)() = sdram_preinit;

} // namespace

#endif
