#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS config for the STM32H7B3I-DK board (Cortex-M7, 280 MHz on the real board,
   64 MHz HSI after reset in Renode — see PRISM_RENODE in main.cpp). This file lives with
   the board support in boards/stm32h7b3i_dk/ — another board gets its own config
   (PRISM's ports/qemu_mps2/ has the QEMU one). */

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
/* SystemCoreClock is the CMSIS variable the clock setup fills in (stm32h7xx_hal_rcc.c
   updates it in HAL_RCC_ClockConfig); under PRISM_RENODE it keeps its reset value 64 MHz.
   It is only used in runtime expressions (the SysTick reload in the port), never in #if. */
extern uint32_t SystemCoreClock;
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                256
#define configTOTAL_HEAP_SIZE                   (384 * 1024) /* heap_4's own static array */
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 1 /* index 0: struct posix_thread * */

/* heap_4's pvPortMalloc uses vTaskSuspendAll, timers need the scheduler state */
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

/* The user LED blinks from the idle hook (freertos_hooks.c), so a black screen still
   shows the firmware is alive. */
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
/* Run-time stats in microseconds (PRISM's ports/freertos/posix_time.c), for the timing line's
   CPU load: the idle task's share of each second (PRISM's ports/freertos/system_counters.cpp).
   64-bit so it never wraps. */
#define configGENERATE_RUN_TIME_STATS           1
#define configRUN_TIME_COUNTER_TYPE             uint64_t
#ifndef __ASSEMBLER__
#include <stdint.h>
#ifdef __cplusplus
extern "C"
#endif
uint64_t prism_now_us(void);
#endif
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
#define portGET_RUN_TIME_COUNTER_VALUE()        prism_now_us()

/* newlib thread safety: same as the QEMU board — each task's TCB embeds a struct _reent
   (include/newlib-freertos.h) and vTaskSwitchContext points newlib's _impure_ptr at the
   running task's copy; PRISM's ports/freertos/newlib_locks.c provides the retargetable locks. */
#define configUSE_NEWLIB_REENTRANT              1

/* Raw NVIC priority register values. The STM32H7 implements 4 priority bits
   (__NVIC_PRIO_BITS == 4 in stm32h7b3xxq.h; FreeRTOS derives configPRIO_BITS from it),
   so 0x50 is priority level 5 shifted into the implemented bits: every interrupt at
   level >= 5 (numerically >= 0x50) may call FreeRTOS FromISR APIs. PendSV/SysTick are
   set to 255 by the port, SVCall to 0. The HAL tick (TIM6) runs at 0x0F — more urgent,
   but it only calls HAL_IncTick(). */
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0x50

extern void board_assert_fail(const char *file, int line);
#define configASSERT(x) \
    do { if (!(x)) board_assert_fail(__FILE__, __LINE__); } while (0)

/* FreeRTOS owns the Cortex-M exception slots 11/14/15; the CMSIS startup
   (startup_stm32h7b3xxq.s) vectors them to these names, so map the port's handlers
   onto them. */
#define vPortSVCHandler        SVC_Handler
#define xPortPendSVHandler     PendSV_Handler
#define xPortSysTickHandler    SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
