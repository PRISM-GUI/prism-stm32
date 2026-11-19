# PRISM on the STM32H7B3I-DK

A port of [PRISM](https://github.com/jeandet/PRISM) to ST's STM32H7B3I-DK board
(Cortex-M7 at 280 MHz, 1.4 MB RAM, 16 MB SDRAM, 480x272 touch screen). PRISM is used
as-is; everything board-specific lives here.

## Status: spike

`spike/main.cpp` lays out a small PRISM model. The goal is to learn whether PRISM's core
cross-compiles, and how big it is.

Build (on the Coder workspace, toolchain in `~/opt/arm-gcc`, PRISM checkout in `~/PRSIM`):

    scripts/remote.sh 'meson setup build --cross-file cross/stm32h7.ini -Dprism_dir=$HOME/PRSIM && ninja -C build'

## Findings

1. The Arm/xPack bare-metal libstdc++ is built with `--enable-threads=single`. It has no
   `std::mutex`, `std::thread` or `std::condition_variable`, and `shared_ptr` counts are not
   atomic (`_S_single` lock policy). PRISM's core uses these in 16 files, and stdexec's
   `run_loop` needs them too. No shim fixes this from outside: the library has to be built
   with thread support.
2. Zephyr's SDK briefly shipped a threaded libstdc++ (0.16.6) and then withdrew it.
3. Smaller issues, already worked around here: Meson's threads dependency adds `-pthread`,
   which the compiler rejects (`scripts/arm-g++` drops it); fmt's file helpers need POSIX
   (`fmt` is used header-only); Meson refuses to compile sources under `subprojects/`
   (`-Dprism_dir`).

## Next

Build our own arm-none-eabi toolchain whose libstdc++ uses POSIX threads (crosstool-NG,
`--enable-threads=posix`), with the pthread calls provided by FreeRTOS through
FreeRTOS-Plus-POSIX. This is what Espressif's ESP-IDF does for its FreeRTOS chips. Then:
a DrawList rasterizer into the LTDC framebuffer, touch input, and the board bring-up from
ST's STM32CubeH7 BSP.
