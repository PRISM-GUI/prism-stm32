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
5. Still to do: a DrawList rasterizer into the LTDC framebuffer, touch input, and the board
   bring-up from ST's STM32CubeH7 BSP.

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
