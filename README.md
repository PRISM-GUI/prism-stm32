# PRISM on the STM32H7B3I-DK

A port of [PRISM](https://github.com/jeandet/PRISM) to ST's STM32H7B3I-DK board
(Cortex-M7 at 280 MHz, 1.4 MB RAM, 16 MB SDRAM, 480x272 touch screen). PRISM is used
as-is (the subproject `subprojects/prism`): its library, its software rasterizer, its frame
runtime (`frame_app`) and its FreeRTOS port (the kernel, the POSIX layer, the QEMU test
firmwares). This repository holds the board: ST's HAL and BSP, the display, the touch, the ADC,
the board's `FreeRTOSConfig.h` and linker script, and the apps (`app/`).

## Status

1. **Toolchain** (`scripts/build-toolchain.sh`, baked into the development image
   `docker/Dockerfile`): GCC 15.2 for the Cortex-M7 whose libstdc++ has threads.
2. **POSIX threads on FreeRTOS** (PRISM's `ports/freertos/`): pthreads, keys, mutexes, condition variables,
   rwlocks, clocks, sleeps and `thread_local` (native ARM TLS, `__aeabi_read_tp`). The QEMU
   tests below pass: 12 of 12 before the newlib work, which added two more (malloc under
   threads, printf from two threads) — 14, now PRISM's `tests/embedded/posix_qemu/`.
3. **newlib thread-safe** (PRISM's `ports/freertos/newlib_locks.c`, `configUSE_NEWLIB_REENTRANT` in the
   board's `FreeRTOSConfig.h`): newlib's retargetable locks are ours (FreeRTOS mutexes,
   created lazily and race-free, no-ops before the scheduler starts), so its no-op
   `libc_a-lock.o` is never linked; each task's TCB embeds a `struct _reent` and
   `_impure_ptr` follows the context switch, so `printf` works from any task (`_write`
   still goes to semihosting).
4. **PRISM's core runs in QEMU** (PRISM's `tests/embedded/prism_qemu/`): a model with a
   hand-written `view()`, a `WidgetTree`, `build_snapshot(480, 272, v)` — geometry, draw
   commands, text; 100 publishes timed; a second thread publishing amid malloc churn.
5. **A software rasterizer for PRISM snapshots** (now PRISM's `raster/`): a `Canvas` over an
   ARGB8888 pixel span (the LTDC framebuffer format), embedded TTF fonts as the text measurer
   (`raster::Fonts`, so layout and drawing agree), and `raster::draw()` painting a
   `SceneSnapshot`. Tested on the host (`host/`) and in QEMU.
6. **The STM32H7B3I-DK board** (`boards/stm32h7b3i_dk/`, `app/`): ST's HAL + BSP bring-up
   (SDRAM, LTDC LCD with double buffering in SDRAM, FT5336 touch on I2C4, LED), FreeRTOS
   with the HAL tick on TIM6, and the apps run by PRISM's `frame_app` — a logic thread polling
   touch and routing it through `WidgetTree`'s input path, a render thread rasterizing
   snapshots into the back buffer and flipping the LTDC layer at vblank. Built as `stm32h7b3i_dk.elf`
   (+ `stm32h7b3i_dk.bin`, size report) and `stm32h7b3i_dk_renode.elf` for Renode
   (`boards/stm32h7b3i_dk/renode/`). See "Run it without the board" and "Flash the board"
   below, and docs/board-bringup.md for the bring-up notes (what was unsure, and why).
   **Tested in Renode** (boots, draws, two taps on "+1" make the counter read 2) and **running
   on the real board** (display and touch work; flashed with `st-flash`).

## How it got here

The first spike found that Arm's prebuilt bare-metal libstdc++ is built with
`--enable-threads=single`: no `std::mutex`, `std::thread`, and non-atomic `shared_ptr` counts.
PRISM uses these in 16 core files, and stdexec too. Zephyr's SDK briefly shipped a threaded
libstdc++ and withdrew it. So this repo builds its own (ESP-IDF does the same for its FreeRTOS
chips) and provides the `pthread_*` calls from FreeRTOS. Smaller fixes: `scripts/arm-g++` drops
`-pthread` (Meson's threads dependency adds it); fmt is used header-only. The POSIX layer, the
rasterizer and the runtime started here and have since moved into PRISM.

Build (on the Coder workspace, where `subprojects/prism` is a symlink to a PRISM checkout):

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini && ninja -C build'

## Third-party sources

ST's HAL, BSP and component drivers, CMSIS and CMSIS-DSP (the scope's FFT) are git
submodules in `third_party/`, pinned to the commits this firmware is built with (FreeRTOS and
stb_truetype come with PRISM, as Meson wraps):

    git clone https://github.com/jeandet/prism-stm32
    cd prism-stm32
    git submodule update --init --depth 1

`third_party/st/` is committed as symlinks into those submodules, laid out like STM32CubeH7's
`Drivers/` tree, because ST's BSP sources include each other by relative path
(`"../Components/ft5336/ft5336.h"`).

## Development image

`docker/Dockerfile` builds an image with everything above: the threaded arm-none-eabi GCC
(built from `scripts/build-toolchain.sh`, ~20 min on 16 cores), meson, ninja and GCC 16 for
the host side, QEMU, openocd and stlink. From the repository root:

    podman build -f docker/Dockerfile -t prism-stm32-dev .

It starts from Fedora 44 by default; `--build-arg BASE=<image>` puts it on another Fedora 44
image (our Coder workspace image, for instance).

The workflow in `.github/workflows/image.yml` publishes it to
`ghcr.io/jeandet/prism-stm32-dev` (tags `latest` and `arm-YYYYMMDD`) when `docker/` or the
toolchain script changes: `podman pull ghcr.io/jeandet/prism-stm32-dev:latest`.

## The firmwares

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini && ninja -C build'
    # -> build/stm32h7b3i_dk.elf, build/stm32h7b3i_dk.bin, build/stm32h7b3i_dk.size.txt,
    #    build/stm32h7b3i_dk_renode.elf

PRISM, cross-compiled, also builds its QEMU test firmwares (mps2-an500) in
`build/subprojects/prism/tests/embedded/`; `meson test -C build` runs them where
`qemu-system-arm` is installed.

## Run it without the board (Renode)

The same firmware runs in [Renode](https://renode.io) 1.17 (portable build in `~/opt/renode`),
with an emulated LCD and touchscreen. `boards/stm32h7b3i_dk/renode/stm32h7b3i_dk.repl` is the
board: Renode's `stm32h7.repl` plus 1 MiB AXI SRAM and the FT5336 touch on I2C4 at 0x38.

First copy the firmware from the Coder workspace (it is built there):

    scripts/fetch-firmware.sh          # -> build-local/

Then, from `boards/stm32h7b3i_dk/renode/`:

    ~/opt/renode/renode run.resc        # a window is the screen; the mouse is your finger
    PRISM_SCREENSHOT=/tmp/shot.png ~/opt/renode/renode --disable-gui --console screenshot.resc
    PRISM_SCREENSHOT=/tmp/tap.png  ~/opt/renode/renode --disable-gui --console tap.resc

`screenshot.resc` runs 5 emulated seconds and saves the screen; `tap.resc` taps "+1" twice
through the emulated touch controller (the counter then shows 2); `widgets.resc` taps the
checkbox and drags the slider (unchecked, volume about 0.86). docs/renode-boot.png,
docs/renode-tap.png and docs/renode-widgets.png are what they produced with
`-Dexample=widgets` on 2026-11-20 (the timing line under the widgets changes from run to run).

The Renode build (`stm32h7b3i_dk_renode.elf`) is the board firmware built with
`-DPRISM_RENODE`, which changes one thing: the LTDC active width is narrowed to the layer's 480
pixels. ST's timings for this panel give a 491-pixel active area around the layer, and
Renode's LTDC model reads the framebuffer with the active width as its line length (a sheared
picture otherwise). Everything else, including ST's 280 MHz clock and SMPS setup, runs as on
the board.

## Flash the board

The image is raw binary at 0x08000000 (the vector table is the first thing in flash):

    scripts/fetch-firmware.sh
    st-flash --reset write build-local/stm32h7b3i_dk.bin 0x08000000

(I am not sure stlink 1.8's `st-flash` handles the H7B3's flash controller unlock sequence —
if it refuses, use the OpenOCD alternative:)

    openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
        -c "program build-local/stm32h7b3i_dk.elf verify reset exit"

What to expect: LED2 (blue, PG2) blinks ~2 Hz from the idle hook even if the screen stays
black; LED3 (red) lit means the touch controller did not answer (the screen still runs, without
input).

The firmware runs one of two examples, chosen with the meson option `example`
(`meson configure build -Dexample=scope` or `widgets`):

- **scope** (the default): a scope on Arduino pin A0 (PA4, ADC1 channel 18), sampled at 16 kHz
  by TIM3 and the DMA. 0 to 3.3 V maps to -1 to 1, so mid-supply reads 0. The board has no
  microphones of its own (its MEMS microphones are an add-on board on the audio connector).
  With nothing wired, A0 floats: touching it shows mains hum. The title says where the samples
  come from ("A0", or "synthetic": a 200 Hz to 4 kHz sweep, used when the ADC does not start,
  and always in Renode), the samples received in the last second, their DC level and peak.
  Left: the last 20 ms; right: the spectrum, 0 to 8 kHz. "freeze" stops the plots, "dB"
  switches the spectrum's scale, "fit" sets both plots' ranges to the signal now, "gain"
  scales both. The bottom line, once a second: frames per second, CPU load, the C heap and
  the RTOS heap in use, then allocations and the time per frame of each stage (the example's
  update, the snapshot build, the drawing and the flip to the LCD). Those are the numbers to
  optimize against.
- **widgets**: the first demo ("PRISM on STM32H7", a counter, "+1", a checkbox, a slider), with
  the same timing line at the bottom.

`host/build/scope_bench` runs the same scope on the PC and prints the same timing line.

Allocation guard: `meson configure build -Dalloc_guard=true` builds a firmware that stops on
any heap allocation once startup is done (the first timing line, one second in). With a
debugger attached it halts in the HardFault handler with the full call stack;
`prism_allocation_first_violation` holds the address that called malloc (the guard is
PRISM's, in `ports/freertos/alloc_count.c`; `main.cpp` arms it at the first timing line). Off by default until
PRISM's frame path is allocation-free (doc/design/allocation-free-frames.md in PRISM).

If it does not boot (black screen, no LED blink): connect the debugger **under reset**
(`openocd ... -c "reset halt"` / `st-flash` with the board held in reset via the NRST pin)
and check `build/stm32h7b3i_dk.size.txt` — the .bss (FreeRTOS's 384 KiB heap_4 is in it)
plus the newlib heap must fit the 1 MiB AXI SRAM. The power supply must be **direct SMPS**
(the build defines `USE_PWR_DIRECT_SMPS_SUPPLY`, as ST's .cproject for this board does): a
wrong supply mode can lock the chip until a power-on reset. A power cycle (unplug/replug)
clears it.

## Profiling

The firmware is built with PRISM's profiling on (the PRISM subproject's `profiling=enabled`,
`profiling_events=1024`, `profiling_threads=6`): the `PRISM_PROFILE_*` marks record into a
global recorder of 1024 events per thread ring × 6 rings — about 144 KiB of .bss in the 1 MiB
AXI SRAM, so check `build/stm32h7b3i_dk.size.txt` after a build. The recorder's clock is the
DWT cycle counter, so timestamps are CPU cycles at 280 MHz.

Press the **blue user button** (PC13) to dump the recorder over the ST-LINK's virtual COM
port (USART1 on PA9/PA10, 921600 baud 8N1 — the `/dev/ttyACM0` the host sees). The dump
pauses the recorder for its duration, so the screen freezes for about **2 s** while
1024 events × 6 threads drain at 921600 baud.

Capture it on the host — start `prism-trace` first, then press the button (it reads until the
dump's end chunk arrives, or 5 s of silence):

    prism-trace /dev/ttyACM0 -o trace.json --baud 921600

The tool's one-line summary also prints the dump's **ticks per second**, which says which
clock the board recorded with: 280000000 when the DWT cycle counter ran, 1000000 for the
microsecond fallback (QEMU/Renode, where the DWT does not count). Then open
<https://ui.perfetto.dev> in a browser and load `trace.json`.
