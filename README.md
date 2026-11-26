# PRISM on two STM32 boards

This repo builds [PRISM](../PRSIM) — a GUI toolkit that renders its own widgets — as firmware for
two discovery boards:

- **STM32H7B3I-DK** (the original port),
- **STM32F746G-DISCO**.

Both run PRISM's embedded example apps unchanged (the widget demo and the audio scope), on the
same 480×272 panel, with a touch screen and one analog input. The board support code is per
board, in `boards/<board>/`; nothing is shared through a HAL abstraction layer.

| | STM32H7B3I-DK | STM32F746G-DISCO |
|---|---|---|
| Chip | STM32H7B3LI, Cortex-M7 at 280 MHz | STM32F746NG, Cortex-M7 at 216 MHz |
| FPU | double precision (`fpv5-d16`) | **single precision only** (`fpv5-sp-d16`) |
| Flash | 2 MiB | 1 MiB |
| RAM used | 1 MiB AXI SRAM | 256 KiB of SRAM (SRAM1+SRAM2) |
| External RAM | 16 MiB SDRAM (IS42S16800J) | 8 MiB SDRAM (MT48LC4M32B2) |
| Panel | 480×272, LTDC | 480×272, LTDC |
| Touch | GT911, FT5336 fallback (I2C4) | FT5336 (I2C3) |
| Analog input | Arduino A0 = PA4 = ADC1 channel 18, 16 bit | Arduino A0 = PA0 = ADC3 channel 0, 12 bit |
| Serial | USART1 through the ST-LINK (PA9/PA10) | USART1 through the ST-LINK (PA9/PB7) |
| LEDs / button | blue LED2 blinks, red LED3 = no touch, user button dumps the profile | green LED1 blinks, key button dumps the profile |
| Widgets demo | 1,025 KB | 365 KB |
| Scope demo | 1,025 KB | 524 KB |

Sizes are `text + data` from `arm-none-eabi-size` on the 2026-11-24 build with the
single-precision toolchain, after the November 2026 size cuts
(`.superpowers/size-audit.md` has the measurements and what each cut was worth). The H7 has no
room problem; the F7 does, which is why its build turns PRISM options off — see
[Building](#building).

Details per board: [docs/boards.md](docs/boards.md). How to flash, read the serial log and debug:
[docs/debugging.md](docs/debugging.md).

## What you need

- Meson (1.5 or newer) and ninja, Python 3, and git.
- An Arm toolchain **per FPU**: `$HOME/opt/arm-gcc-threads` for the H7 and
  `$HOME/opt/arm-gcc-threads-sp` for the F7. `cross/stm32h7.ini` and `cross/stm32f7.ini` point at
  them through `scripts/arm-gcc` / `scripts/arm-gcc-sp`.
- `st-flash` (stlink tools) to flash, OpenOCD to debug, Renode to run without a board.
- A submodule checkout: `git submodule update --init --recursive`.

The toolchains are ordinary GCC builds with POSIX threads enabled, because PRISM's platform layer
uses pthreads on FreeRTOS. Build them with:

    scripts/build-toolchain.sh                    # H7: fpv5-d16 → ~/opt/arm-gcc-threads
    FPU=fpv5-sp-d16 PREFIX=$HOME/opt/arm-gcc-threads-sp \
        WORK=$HOME/toolchain-build-sp scripts/build-toolchain.sh    # F7

Each install carries one library variant, built for that FPU (`--disable-multilib`).

**The F7 cannot use the H7's libraries.** They link — both are hard float — and then fault: the
H7's double maths (log10, formatting a double, …) is compiled to double-precision FPU
instructions, which the F7's FPU does not have.

## Building

Everything goes through `scripts/remote.sh`, which syncs the repo to a Coder workspace and runs the
command there with the toolchains on `PATH`:

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini && ninja -C build'
    scripts/remote.sh 'meson setup build-f7 --cross-file cross/stm32f7.ini \
        -Dboard=stm32f746g_disco && ninja -C build-f7'

With both toolchains installed locally, plain `meson setup` and `ninja` work the same way. The
cross file, not the board option, selects the CPU instruction set.

Meson options of this project:

| option | values | default | what it does |
|---|---|---|---|
| `board` | `stm32h7b3i_dk`, `stm32f746g_disco` | H7 | which board directory and linker script |
| `example` | `widgets`, `scope` | `scope` | which PRISM example the firmware runs |
| `alloc_guard` | `true`, `false` | `false` | trap on any heap allocation after startup |

The output is `<board>.elf`, `<board>_renode.elf`, `<board>.bin` and `<board>.size.txt` in the
build directory. `scripts/fetch-firmware.sh <board> <build-dir>` copies the three images to
`build-local/`.

The F7 build also sets PRISM's own options, because 1 MiB of flash needs them:

| PRISM option | F7 | desktop and H7 |
|---|---|---|
| `fmt_locale` | `false` — no `{:L}` localized formatting | `true` |
| `line_breaking` | `simple` — no libunibreak, breaks at spaces and around CJK | `unicode` |
| `embedded_bold_face` | `false` — bold text uses the regular face | `true` |
| `profiling_events` / `profiling_threads` | 128 / 2 | 1024 / 6 |

Both cross files also set `b_ndebug = 'true'`, which drops `assert` and PRISM's `#ifndef NDEBUG`
diagnostics. PRISM's always-on contract checks are explicit `std::abort()` paths, so they stay.

**Subproject options are read once, when the build directory is configured.** Changing
`fmt_locale`, `line_breaking`, `embedded_bold_face` or the profiling sizes in `meson.build` needs a
fresh directory:

    rm -rf build-f7 && meson setup build-f7 --cross-file cross/stm32f7.ini -Dboard=stm32f746g_disco

`meson setup --reconfigure` and `meson setup --wipe` keep the old values.

## Flash a board

The image is raw binary at 0x08000000; the vector table is the first thing in flash.

    scripts/fetch-firmware.sh stm32f746g_disco build-f7
    st-flash --reset write build-local/stm32f746g_disco.bin 0x08000000

For the H7 use `stm32h7b3i_dk`. `st-flash` 1.8 may not unlock the H7B3's flash controller; if it
refuses, OpenOCD programs the ELF instead (see [docs/debugging.md](docs/debugging.md)).

Expected on a healthy board: the screen paints the demo within a second, and the green LED
(F7) or blue LED2 (H7) blinks about twice a second from the RTOS idle hook. On the H7 a lit red
LED3 means the touch controller did not answer; the screen still runs, without input.

## The apps

Both apps are PRISM's, built from the submodule with no change: `examples/embedded/widgets_main.cpp`
and `examples/embedded/scope_main.cpp` (meson option `example`). A board's `main.cpp` is its port:
it brings the board up, registers the panel and the touch as PRISM's frame sink and input, registers
the ADC on A0 as PRISM's capture device, then runs the app's own `main()` on the same FreeRTOS task
— 32 KiB of stack, as before.

- **widgets**: a counter, a "+1" button, a checkbox and a slider, plus a timing line. The quickest
  way to check that drawing and touch work.
- **scope**: a scope on PRISM's `audio::Input`, 16 kHz. Left: the last 20 ms. Right: the spectrum
  up to 8 kHz. The title says where the samples come from — `A0`, or `synthetic` (a sweep, used when
  the ADC is not registered and always in Renode). Buttons: `freeze`, `dB`, `fit`, `gain`.

The bottom line of both apps is PRISM's frame statistics: frames per second, CPU load, heap
use, allocations per frame, and the time each stage took — update, snapshot, draw, present. Those
are the numbers to optimize against. The port sends that line over COM1, and arms the allocation
guard (`alloc_guard`) on the first one.

## Run it without a board (Renode)

Renode runs the `*_renode.elf` image, which is the board firmware built with `-DPRISM_RENODE`.
Screenshots come from the emulated LTDC, and the emulated FT5336 takes taps:

    cd boards/stm32f746g_disco/renode
    PRISM_SCREENSHOT=/tmp/widgets.png ~/opt/renode/renode --disable-gui --console \
        -e '$bin = @/path/to/build-f7/stm32f746g_disco_renode.elf' screenshot.resc

`run.resc` opens the GUI, `screenshot.resc` saves the screen after 5 s, `tap.resc` taps "+1" twice
and saves the counter, `widgets.resc` ticks the checkbox and drags the slider.

What the emulation does **not** give you: caches (writes are visible immediately, so cache bugs stay
hidden), the FMC/SDRAM controller (plain memory, no timing, so bring-up bugs stay hidden), the
DMA2D (the emulated F7 clears on the CPU), the ADC (the scope uses the synthetic source), the DWT
on the F7 platform (its clock does not count, so the scope stays on "measuring…"), and the FPU
precision rule (double-precision instructions do not fault there — only the board shows that).

## Board pitfalls worth knowing

1. **The SDRAM comes up before the C++ constructors.** Both heaps are in SDRAM, and constructors
   allocate (libstdc++'s emergency exception buffer is a malloc), so the F7 brings the board up
   from a `.preinit_array` entry, not from `main()`. Otherwise the first malloc uses a heap that
   never really wrote — the symptom is a white screen.
2. **The SDRAM is mapped write-back.** Cortex-M7 erratum 1259864 makes write-through unsafe here (a
   load can miss the last store; it corrupted malloc's heap, see commit `33d8f57`). The display
   therefore cleans the data cache before the LTDC or the DMA2D reads a buffer, and invalidates it
   after the DMA2D writes one.
3. **One toolchain per FPU.** See above: the F7's libraries must be built for `fpv5-sp-d16`.
4. **`HAL_Delay` needs `HAL_Init`.** ST's SDRAM init waits for the controller, so it cannot run in
   `SystemInit`; it runs after the clocks, in the same early board code.
5. **The startup's `main` is the app's.** The CMSIS startup is a submodule and calls `main`, and
   `main` is now PRISM's example. `-Wl,--wrap=main` sends that call to `boards/<board>/entry.c`:
   `board_main()` is the board's entry, and `board_app_main()` reaches the app's `main()` once the
   port is registered.

## Where things are

    boards/stm32h7b3i_dk/     H7 board: the port (main.cpp + entry.c), display, touch, ADC capture, UART, linker script, Renode
    boards/stm32f746g_disco/  F7 board: the same, plus boot_log (serial progress and fault report)
    app/adc_sample.hpp        raw 16-bit ADC sample → -1..1 (the H7 board's ADC; the F7's is 12-bit)
    cross/                    the two meson cross files, one per FPU
    scripts/                  toolchain build, remote build, firmware fetch, size reports
    docs/                     board notes, debugging notes, diagrams
    third_party/              ST HAL, CMSIS, BSP components, as submodules

PRISM itself is a subproject (`subprojects/prism.wrap`) with its own repository; this repo only
carries the board code and the build glue.

Diagrams: [F7 memory map](docs/diagrams/memory-map-f7.svg),
[boot order](docs/diagrams/boot-sequence.svg), [one frame through frame_app](docs/diagrams/frame-path.svg).
