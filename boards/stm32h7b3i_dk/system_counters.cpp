// The timing line's CPU and memory figures on the board: the FreeRTOS idle task's run time
// (configGENERATE_RUN_TIME_STATS, counted in microseconds by posix/posix_time.c), newlib's
// heap (malloc, from `end` up to _heap_end: stm32h7b3.ld) and FreeRTOS's heap_4.

#include "app/runtime.hpp"

#include "FreeRTOS.h"
#include "task.h"

#include <malloc.h>

extern "C" char end;       // stm32h7b3.ld: where the newlib heap starts
extern "C" char _heap_end; // and where it must stop (sbrk.c)

std::optional<app::SystemCounters> app::system_counters()
{
    const struct mallinfo heap = mallinfo();
    return SystemCounters{
        .idle_us = ulTaskGetIdleRunTimeCounter(),
        .heap_used = std::size_t(heap.uordblks),
        .heap_total = std::size_t(&_heap_end - &end),
        .stacks_used = configTOTAL_HEAP_SIZE - xPortGetFreeHeapSize(),
        .stacks_total = configTOTAL_HEAP_SIZE,
    };
}
