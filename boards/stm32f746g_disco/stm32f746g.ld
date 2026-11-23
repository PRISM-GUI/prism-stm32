/* STM32F746G-DISCO (STM32F746NGHx) linker script.

   From ST's STM32F746NGHx_FLASH.ld, plus what PRISM's ports/qemu_mps2/mps2_an500.ld adds:
   the EXTERN list of posix symbols (gthr-posix references them weakly, so --gc-sections
   would drop them), .tdata/.tbss with their symbols, KEEP(.init/.fini), .eh_frame kept,
   and the init/fini arrays.

   Memory map of the STM32F746G-DISCO as used here:
     FLASH     0x08000000  1 MiB   - code, vectors, the load image of .data/.tdata
     SRAM      0x20010000  256 KiB - SRAM1 (240 KiB) + SRAM2 (16 KiB), contiguous:
                                     .data/.bss, FreeRTOS heap_4, the newlib heap (_sbrk
                                     grows from `end` to _heap_end), the initial stack at
                                     the top. The DTCM at 0x20000000 (SRAM2 is behind it
                                     in the address map) is left out on purpose: the DMA
                                     cannot reach it, and DMA buffers (the ADC's) must.
     SDRAM     0xC0000000  8 MiB   - NOT in the linker script: it only exists after
                                     BSP_SDRAM_Init, so the LTDC framebuffers are plain
                                     addresses there (0xC0000000 and 0xC0080000,
                                     480*272*4 = 522240 bytes each; see display.cpp). */

/* The posix layer is referenced weakly by libstdc++ (gthr-posix), so --gc-sections
   would drop it; EXTERN makes every one of these symbols a GC root. */
EXTERN(
    pthread_create pthread_join pthread_detach pthread_self pthread_equal
    pthread_cancel pthread_exit pthread_once
    pthread_key_create pthread_key_delete pthread_getspecific pthread_setspecific
    pthread_attr_init pthread_attr_destroy pthread_attr_setstacksize
    pthread_attr_getstacksize pthread_attr_setdetachstate pthread_attr_getdetachstate
    pthread_mutex_init pthread_mutex_destroy pthread_mutex_lock pthread_mutex_trylock
    pthread_mutex_unlock pthread_mutex_timedlock
    pthread_mutexattr_init pthread_mutexattr_destroy pthread_mutexattr_settype
    pthread_mutexattr_gettype
    pthread_cond_init pthread_cond_destroy pthread_cond_wait pthread_cond_timedwait
    pthread_cond_signal pthread_cond_broadcast
    pthread_condattr_init pthread_condattr_destroy pthread_condattr_setclock
    pthread_condattr_getclock
    pthread_rwlock_init pthread_rwlock_destroy pthread_rwlock_rdlock
    pthread_rwlock_tryrdlock pthread_rwlock_timedrdlock pthread_rwlock_wrlock
    pthread_rwlock_trywrlock pthread_rwlock_timedwrlock pthread_rwlock_unlock
    sched_yield clock_gettime nanosleep sleep usleep gettimeofday _gettimeofday time
    __aeabi_read_tp __cxa_thread_atexit _write
)

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20010000, LENGTH = 256K    /* SRAM1 + SRAM2 */
}

/* Initial stack pointer: top of SRAM. The main stack gets the 16 KiB below it
   (the startup code and the pre-scheduler context run on it); the newlib heap stops
   at _heap_end, 16 KiB below that (see PRISM's ports/freertos/sbrk.c). */
_estack = ORIGIN(RAM) + LENGTH(RAM);
_heap_end = _estack - 16K;

SECTIONS
{
    /* The startup code (CMSIS startup_stm32h7b3xxq.s) into FLASH: vectors first. */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        KEEP(*(.init)) KEEP(*(.fini))   /* crti/crtn pieces of _init/_fini: unreferenced, never collected */
        *(.text*)
        *(.rodata*)
        *(.glue_7) *(.glue_7t)
        . = ALIGN(8);
        _etext = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    /* crtbegin.o refers to .eh_frame (frame_dummy): kept, not discarded. */
    .eh_frame :
    {
        KEEP(*(.eh_frame))
    } > FLASH

    .preinit_array :
    {
        PROVIDE(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array))
        PROVIDE(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*))) KEEP(*(.fini_array))
        PROVIDE(__fini_array_end = .);
    } > FLASH

    /* Used by the startup (startup_stm32h7b3xxq.s) to initialize .data. */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(8);
        _sdata = .;
        *(.data*)
        *(.got*)
        . = ALIGN(8);
        _edata = .;
    } > RAM AT> FLASH

    /* TLS template: copied per thread by PRISM's ports/freertos/tls.c from its load image
       (_tdata_lma), so the startup does not copy it (the CMSIS startup only copies .data). */
    .tdata :
    {
        . = ALIGN(8);
        _tdata_start = .;
        *(.tdata .tdata.*)
        _tdata_end = .;
        _etdata = .;
        _tdata_lma = LOADADDR(.tdata);
    } > RAM AT> FLASH

    .tbss (NOLOAD) :
    {
        _tbss_start = .;
        *(.tbss .tbss.*)
        _tbss_end = .;
    } > RAM

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    . = ALIGN(8);
    PROVIDE(end = .);   /* newlib's _sbrk (PRISM's ports/freertos/sbrk.c) grows the C heap from here */
    PROVIDE(_end = .);

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
