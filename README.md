# PRISM on the STM32H7B3I-DK

A port of [PRISM](https://github.com/jeandet/PRISM) to ST's STM32H7B3I-DK board
(Cortex-M7 at 280 MHz, 1.4 MB RAM, 16 MB SDRAM, 480x272 touch screen). PRISM is used
as-is; everything board-specific lives here.

## Status

1. **Toolchain** (`scripts/build-toolchain.sh`, also baked into the Coder image
   `coder-embedded`): GCC 15.2 for the Cortex-M7 whose libstdc++ has threads.
2. **POSIX threads on FreeRTOS** (`posix/`): pthreads, keys, mutexes, condition variables,
   rwlocks, clocks, sleeps and `thread_local` (native ARM TLS, `__aeabi_read_tp`). The QEMU
   tests below pass: 12 of 12.
3. **PRISM's core cross-compiles** only as far as the spike went (`spike/main.cpp`); with the
   threaded toolchain it is the next step.
4. Still to do: a DrawList rasterizer into the LTDC framebuffer, touch input, and the board
   bring-up from ST's STM32CubeH7 BSP.

Known gap: newlib's `printf` prints nothing from a FreeRTOS task yet (its per-task state,
`configUSE_NEWLIB_REENTRANT`, is not set up); the tests log through semihosting directly.

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

## QEMU tests of the posix/ layer (mps2-an500)

`posix/` implements the POSIX threads/mutex/condvar/rwlock/time API that the
threaded libstdc++ (gthr-posix) calls, on top of the FreeRTOS kernel in
`third_party/FreeRTOS-Kernel`, plus native `thread_local` support (`__aeabi_read_tp`).
`tests/qemu/` is a firmware for QEMU's MPS2-AN500 board (Cortex-M7) that runs C++
tests (`std::thread`, `std::mutex`, `condition_variable`, `thread_local`,
`shared_ptr`, `recursive_mutex`, `timed_mutex`, `shared_mutex`, sleeps) over
semihosting; QEMU's exit status is the number of failed tests.

Build (same as the spike, the cross file already passes `--specs=nosys.specs`):

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini -Dprism_dir=$HOME/PRSIM && ninja -C build'

Run:

    qemu-system-arm -M mps2-an500 -nographic -semihosting-config enable=on,target=native -kernel build/qemu_tests.elf

Exit codes: 0 = all tests passed; 1..10 = that many test failures; 90 = configASSERT,
91 = malloc-failed hook, 93 = stack overflow, 94 = unexpected exception, 99 = watchdog
(20 s).
