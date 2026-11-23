/* STM32F746G-DISCO (STM32F746NGHx) linker script.

   From ST's STM32F746NGHx_FLASH.ld, plus what PRISM's ports/qemu_mps2/mps2_an500.ld adds:
   the EXTERN list of posix symbols (gthr-posix references them weakly, so --gc-sections
   would drop them), .tdata/.tbss with their symbols, KEEP(.init/.fini), .eh_frame kept,
   and the init/fini arrays.

   Memory map of the STM32F746G-DISCO as used here:
     FLASH     0x08000000  1 MiB   - code, vectors, the load image of .data/.tdata
     SRAM      0x20010000  256 KiB - SRAM1 (240 KiB) + SRAM2 (16 KiB), contiguous:
                                     .data/.bss and the initial stack at the top. The DTCM
                                     at 0x20000000 (SRAM2 is behind it in the address map) is
                                     left out on purpose: the DMA cannot reach it, and DMA
                                     buffers (the ADC's) must.
     SDRAM     0xC0100000  7 MiB   - brought up by BSP_SDRAM_Init before the app runs (see
                                     display.cpp), 8 MiB total: the two LTDC framebuffers at
                                     0xC0000000 and 0xC0080000 (480*272*4 = 522240 bytes
                                     each) are plain addresses, then this region holds
                                     heap_4's 1 MiB pool (.sdram_heap) and the newlib C heap
                                     up to the top of the SDRAM. */

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
    /* SDRAM above the two framebuffers (0xC0000000..0xC0100000, plain addresses in
       display.cpp): FreeRTOS's heap_4 pool, then newlib's C heap. */
    SDRAM (rwx) : ORIGIN = 0xC0100000, LENGTH = 7M
}

/* Initial stack pointer: top of SRAM. The main stack grows down from it (the startup code and
   the pre-scheduler context run on it, and every interrupt after). */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* Both heaps are in the SDRAM, which main.cpp's MPU_Config maps cacheable write-back, so
   they are normal cached memory. SRAM cannot hold them: PRISM's frame runtime alone asks for a
   64 KiB logic thread plus a 48 KiB render thread, and its C++ allocations on top. Nothing that
   DMA reads or writes lives in either heap.
   - .sdram_heap, below, is heap_4's pool (ucHeap, freertos_hooks.c);
   - the C heap is what PRISM's ports/freertos/sbrk.c grows from `end` up to _heap_end. */
_heap_end = ORIGIN(SDRAM) + LENGTH(SDRAM);

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
    /* heap_4's pool. NOLOAD: it has no initial contents (heap_4 formats it at the first
       pvPortMalloc). The C heap starts at the next MiB above it. */
    .sdram_heap (NOLOAD) :
    {
        _sdram_heap_start = .;
        KEEP(*(.sdram_heap))
        _sdram_heap_end = .;
    } > SDRAM

    _heap_start = ALIGN(_sdram_heap_end, 1M);

    /* `end` is where PRISM's _sbrk starts the C heap. It is the SDRAM above the framebuffers
       and above heap_4's pool, not the end of .bss: see _heap_start above. */
    PROVIDE(end = _heap_start);
    PROVIDE(_end = .);

    /DISCARD/ :
    {
        *(.note*)
        *(.comment)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
