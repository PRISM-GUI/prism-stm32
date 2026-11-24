# The two boards

Both boards run the same PRISM build and the same demos. What differs is the silicon, the memory
map, the drivers, and how much of PRISM fits.

## Side by side

| | STM32H7B3I-DK | STM32F746G-DISCO |
|---|---|---|
| MCU | STM32H7B3LI, Cortex-M7 r1p0 | STM32F746NG, Cortex-M7 r0p1 |
| Clock | 280 MHz from the 24 MHz HSE | 216 MHz from the 25 MHz HSE (over-drive on) |
| FPU | double precision, `fpv5-d16` | single precision, `fpv5-sp-d16` |
| Internal flash | 2 MiB | 1 MiB |
| Internal RAM in use | 1 MiB AXI SRAM at 0x24000000 | 256 KiB SRAM1+SRAM2 at 0x20010000 |
| External RAM | 16 MiB SDRAM, IS42S16800J | 8 MiB SDRAM, MT48LC4M32B2 |
| Framebuffers | 0xD0000000 and 0xD0080000 | 0xC0000000 and 0xC0080000 |
| RTOS heap | 384 KiB inside the AXI SRAM | 1 MiB in SDRAM, `.sdram_heap` |
| C heap | newlib's window below the stack, in AXI SRAM | 6 MiB in SDRAM above the RTOS pool |
| Panel | 480×272 RK043FN48H over LTDC | 480×272 RK043FN48H over LTDC |
| Pixel clock | LTDC from PLL3 | 9.6 MHz from PLLSAI (192 / 5, ÷ 4) |
| Touch | GT911 on I2C4, FT5336 fallback | FT5336 on I2C3 (PH7/PH8, address 0x70) |
| Analog input | PA4 = ADC1 channel 18, 16 bit, TIM3 trigger | PA0 = ADC3 channel 0, 12 bit, TIM2 trigger |
| Console | USART1 PA9/PA10 through the ST-LINK | USART1 PA9/PB7 through the ST-LINK |
| LEDs | blue LED2 blinks, red LED3 = touch failed | green LED1 blinks |
| Profile dump button | user button, PC13 | key button, PI11 |

The F7's SRAM is 320 KiB in total, but the 64 KiB of DTCM at 0x20000000 are left out: the DMA
cannot reach them, and the ADC's buffer must be DMA-reachable. That is why the linker script starts
RAM at 0x20010000.

## The H7 board

The H7 came first. It has room for everything: 2 MiB of flash, 1 MiB of AXI SRAM, and PRISM's
profiling recorder (1024 events × 6 thread rings, about 144 KiB of `.bss`) fits next to a 384 KiB
RTOS heap.

- Board files: `boards/stm32h7b3i_dk/`. ST's BSP is used for the display, touch and SDRAM
  (`stm32h7b3i_discovery*.c`), with the framebuffers placed in SDRAM by `is42s16800j_conf.h`.
- `stm32h7b3i_dk.repl` plus `run.resc`, `screenshot.resc`, `tap.resc`, `widgets.resc` in
  `boards/stm32h7b3i_dk/renode/`.
- The ADC reads Arduino A0 with 16 bits, DMA-driven at 16 kHz, mapped to -1..1 as
  `(raw - 32768) / 32768`.

## The F7 board

The F7 is the tight one: 1 MiB of flash and 256 KiB of usable SRAM, against a GUI toolkit that
wants both. Two consequences shape the board code.

**Both heaps live in the 8 MiB SDRAM.** PRISM's frame runtime starts a 64 KiB logic thread and a
48 KiB render thread, and the app task asks for 32 KiB more; that does not fit in SRAM next to
PRISM's statics. So heap_4's pool is application-allocated (`ucHeap` in `.sdram_heap`, 1 MiB at
0xC0100000) and newlib's `_sbrk` window starts above it (0xC0200000 to the top of the SDRAM).
SRAM keeps only `.data`, `.bss` and the main stack. See
[diagrams/memory-map-f7.svg](diagrams/memory-map-f7.svg).

**The board is brought up before the C++ constructors.** Constructors allocate, and the moment they
do, malloc has to write to SDRAM. So `board_early_init()` runs from a `.preinit_array` entry: boot
log at 16 MHz, MPU and caches, `HAL_Init`, clocks, SDRAM, then a full write/read test of the 8 MiB.
Only then do the constructors and `main()` run. See
[diagrams/boot-sequence.svg](diagrams/boot-sequence.svg).

Other F7 specifics:

- The display, touch and ADC drivers are hand-written in `boards/stm32f746g_disco/`: ST's F7 BSP
  for this board needs the font utilities (not vendored) and an older touch component API, so only
  its LED/button/COM/SDRAM parts are used.
- `boot_log.cpp` prints progress and fault reports on the ST-LINK's COM port, before HAL, clocks or
  the RTOS exist. That is the first thing to read when the screen stays dark.
- Renode skips the clock setup (its PWR model has no over-drive handshake) and the SDRAM init (no
  FMC model — the SDRAM is plain mapped memory).

## Flash and RAM, after the size cuts

| image | text | data | fits in |
|---|---|---|---|
| F7 widgets | 365 KB | 1,732 | 1 MiB, with ~660 KB to spare |
| F7 scope | 524 KB | 18,136 | 1 MiB, with ~500 KB to spare |
| H7 (scope) | 1,004,644 | 21,080 | 2 MiB |

Measured on 2026-11-24 with the single-precision toolchain (`scripts/arm-gcc-sp`), after the
four cuts described in `.superpowers/size-audit.md`. The F7
build gets there with PRISM's `fmt_locale=false`, `line_breaking=simple` and
`embedded_bold_face=false` options plus NDEBUG; the H7 and desktop keep the defaults.
