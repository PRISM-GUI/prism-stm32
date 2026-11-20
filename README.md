# PRISM on the STM32H7B3I-DK

A port of [PRISM](https://github.com/jeandet/PRISM) to ST's STM32H7B3I-DK board
(Cortex-M7 at 280 MHz, 1.4 MB RAM, 16 MB SDRAM, 480x272 touch screen). PRISM is used
as-is; everything board-specific lives here.

## Status

1. **Toolchain** (`scripts/build-toolchain.sh`, also baked into the Coder image
   `coder-embedded`): GCC 15.2 for the Cortex-M7 whose libstdc++ has threads.
2. **POSIX threads on FreeRTOS** (`posix/`): pthreads, keys, mutexes, condition variables,
   rwlocks, clocks, sleeps and `thread_local` (native ARM TLS, `__aeabi_read_tp`). The QEMU
   tests below pass: 12 of 12 before the newlib work, which added two more (malloc under
   threads, printf from two threads) — 14 in `tests/posix_qemu/`.
3. **newlib thread-safe** (`posix/newlib_locks.c`, `configUSE_NEWLIB_REENTRANT` in the
   board's `FreeRTOSConfig.h`): newlib's retargetable locks are ours (FreeRTOS mutexes,
   created lazily and race-free, no-ops before the scheduler starts), so its no-op
   `libc_a-lock.o` is never linked; each task's TCB embeds a `struct _reent` and
   `_impure_ptr` follows the context switch, so `printf` works from any task (`_write`
   still goes to semihosting).
4. **PRISM's core runs in QEMU** (`tests/prism_qemu/`, `prism_tests.elf`): a model with a
   hand-written `view()`, a `WidgetTree`, `build_snapshot(480, 272, v)` — geometry, draw
   commands, text; 100 publishes timed; a second thread publishing amid malloc churn.
5. **A software rasterizer for PRISM snapshots** (`raster/`): a `Canvas` over an ARGB8888
   pixel span (the LTDC framebuffer format), embedded TTF fonts as the text measurer
   (`raster::Fonts`, so layout and drawing agree), and `raster::draw()` painting a
   `SceneSnapshot`. Tested on the host (`host/`) and in QEMU.
6. **The STM32H7B3I-DK board** (`boards/stm32h7b3i_dk/`, `app/`): ST's HAL + BSP bring-up
   (SDRAM, LTDC LCD with double buffering in SDRAM, FT5336 touch on I2C4, LED), FreeRTOS
   with the HAL tick on TIM6, and the demo app — a logic thread polling touch and routing
   it through `WidgetTree`'s input path, a render thread rasterizing snapshots into the
   back buffer and flipping the LTDC layer at vblank. Built as `stm32h7b3i_dk.elf`
   (+ `stm32h7b3i_dk.bin`, size report) and `stm32h7b3i_dk_renode.elf` for Renode
   (`boards/stm32h7b3i_dk/renode/`). See "Run it without the board" and "Flash the board"
   below, and docs/board-bringup.md for the bring-up notes (what was unsure, and why).
   **Tested in Renode** (boots, draws, two taps on "+1" make the counter read 2);
   **not yet on the real board**.

## How it got here

The first spike found that Arm's prebuilt bare-metal libstdc++ is built with
`--enable-threads=single`: no `std::mutex`, `std::thread`, and non-atomic `shared_ptr` counts.
PRISM uses these in 16 core files, and stdexec too. Zephyr's SDK briefly shipped a threaded
libstdc++ and withdrew it. So this repo builds its own (ESP-IDF does the same for its FreeRTOS
chips) and provides the `pthread_*` calls from FreeRTOS. Smaller fixes: `scripts/arm-g++` drops
`-pthread` (Meson's threads dependency adds it); fmt is used header-only; PRISM's sources come
from `-Dprism_dir` because Meson refuses files under `subprojects/`.

Build (on the Coder workspace):

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini -Dprism_dir=$HOME/PRSIM && ninja -C build'

## Third-party sources

`third_party/` is not committed. Besides ST's and FreeRTOS's repositories, the audio scope uses
CMSIS-DSP's real FFT:

    git clone --depth 1 --branch v1.16.2 https://github.com/ARM-software/CMSIS-DSP third_party/CMSIS-DSP

## QEMU board support and the test firmwares (mps2-an500)

`boards/qemu_mps2/` is the board support for QEMU's MPS2-AN500 board (Cortex-M7), shared by
every firmware for that board: `startup.c` (vectors, Reset_Handler), `semihost.c` (log/exit
and newlib's `_write`), `freertos_hooks.c`, `FreeRTOSConfig.h` and the `mps2_an500.ld` linker
script. `posix/` implements the POSIX threads/mutex/condvar/rwlock/time API that the threaded
libstdc++ (gthr-posix) calls, on top of the FreeRTOS kernel in `third_party/FreeRTOS-Kernel`,
plus native `thread_local` support (`__aeabi_read_tp`) and newlib's retargetable locks.

Two firmwares are built from it:

- `posix_tests.elf` (`tests/posix_qemu/main.cpp`): C++ tests (`std::thread`, `std::mutex`,
  `condition_variable`, `thread_local`, `shared_ptr`, `recursive_mutex`, `timed_mutex`,
  `shared_mutex`, sleeps, plus malloc stress and printf from two threads) over semihosting;
  QEMU's exit status is the number of failed tests.
- `prism_tests.elf` (`tests/prism_qemu/main.cpp`): PRISM's core (`prism_core`) in QEMU — a
  model, a `WidgetTree`, snapshots, timed publishes, a second thread publishing.

Run:

    qemu-system-arm -M mps2-an500 -nographic -semihosting-config enable=on,target=native -kernel build/posix_tests.elf
    qemu-system-arm -M mps2-an500 -nographic -semihosting-config enable=on,target=native -kernel build/prism_tests.elf

Exit codes: 0 = all tests passed; 1..N = that many test failures; 90 = configASSERT,
91 = malloc-failed hook, 93 = stack overflow, 94 = unexpected exception, 99 = watchdog
(20 s for the posix tests, 60 s for the PRISM tests).

The board firmwares build in the same `build/`:

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini -Dprism_dir=$HOME/PRSIM && ninja -C build'
    # -> build/stm32h7b3i_dk.elf, build/stm32h7b3i_dk.bin, build/stm32h7b3i_dk.size.txt,
    #    build/stm32h7b3i_dk_renode.elf

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
docs/renode-tap.png and docs/renode-widgets.png are what they produced on 2026-11-20.

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

- **scope** (the default): the audio scope. The title says where the samples come from:
  "mic" (the board's MEMS microphone 1, 16 kHz) or "synthetic" (a 200 Hz to 4 kHz sweep, used
  when the microphones do not start, and always in Renode). Left: the last 20 ms of samples;
  right: the spectrum, 0 to 8 kHz. "freeze" stops the plots, "dB" switches the spectrum's
  scale, "gain" scales both. Whistling should move the spectrum's peak. The bottom line is the
  timing, once a second: frames per second, then per frame the snapshot build, the drawing and
  the flip to the LCD. Those are the numbers to optimize against.
- **widgets**: the first demo ("PRISM on STM32H7", a counter, "+1", a checkbox, a slider).

`host/build/scope_bench` runs the same scope on the PC and prints the same timing line.

If it does not boot (black screen, no LED blink): connect the debugger **under reset**
(`openocd ... -c "reset halt"` / `st-flash` with the board held in reset via the NRST pin)
and check `build/stm32h7b3i_dk.size.txt` — the .bss (FreeRTOS's 384 KiB heap_4 is in it)
plus the newlib heap must fit the 1 MiB AXI SRAM. The power supply must be **direct SMPS**
(the build defines `USE_PWR_DIRECT_SMPS_SUPPLY`, as ST's .cproject for this board does): a
wrong supply mode can lock the chip until a power-on reset. A power cycle (unplug/replug)
clears it.
