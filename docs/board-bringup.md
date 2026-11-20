# PRISM on the STM32H7B3I-DK: board firmware + the same firmware in Renode

(History: the bring-up notes as written. Since then the runtime, the rasterizer and the POSIX
layer have moved into PRISM, `app/demo.hpp` became `app/widgets_app.hpp`, and the build takes
PRISM as a subproject, without `-Dprism_dir`: README.md is current.)

Work done in `/home/jeandet/Documents/prog/prism-stm32` (no commits; PRSIM untouched).
No shell in this session — only read/edit/write — so **nothing was built or run here**; the
controller builds, runs Renode and flashes the board. Everything below about behaviour is
expected from reading the code, not measured.

## 1. What was added

```
boards/stm32h7b3i_dk/
  stm32h7xx_hal_conf.h            from ST's cube-example, trimmed to RCC/PWR/FLASH/CORTEX/
                                 GPIO/EXTI/DMA/DMA2D/LTDC/SDRAM/I2C/MDMA/TIM; TICK_INT_PRIORITY 0x0F
  stm32h7b3i_discovery_conf.h    from the example; LCD_LAYER_0_ADDRESS 0xD0000000,
                                 LCD_LAYER_1_ADDRESS 0xD0080000, USE_TS_GESTURE 0
  ft5336_conf.h / gt911_conf.h   panel geometry 480x272 (from the *_conf_template.h)
  is42s16800j_conf.h             REFRESH_COUNT recalculated for SDCLK = HCLK/3 = 93.3 MHz
                                 (MX_SDRAM_BANK2_Init uses FMC_SDRAM_CLOCK_PERIOD_3): 1438,
                                 not the template's 0x0603 (100 MHz) which would refresh too
                                 slowly at 93.3 MHz and lose SDRAM data
  FreeRTOSConfig.h               like boards/qemu_mps2's but configCPU_CLOCK_HZ = SystemCoreClock,
                                 heap 384 KiB, idle hook on, SysTick/SVC/PendSV mapped onto the
                                 CMSIS vector names, configMAX_SYSCALL_INTERRUPT_PRIORITY 0x50
  stm32h7xx_hal_timebase_tim.c   ST's timebase_tim_template on TIM6 (handle non-static)
  stm32h7xx_it.c                 TIM6_DAC_IRQHandler -> HAL_TIM_IRQHandler; HardFault parks
  sbrk.c                         newlib _sbrk: heap in AXI SRAM from `end` to `_heap_end`, ENOMEM at the limit
  freertos_hooks.c               fault hooks park; LED2 blinks 2 Hz from vApplicationIdleHook
  stm32h7b3.ld                    ST's STM32H7B3LIHXQ_FLASH.ld + mps2_an500.ld's additions
                                 (EXTERN posix list, .tdata/.tbss, KEEP init/fini/eh_frame, arrays)
  display.hpp/.cpp               Display : app::FrameSink — SDRAM+LCD init, back(), present()
  touch.hpp/.cpp                 Touch : app::TouchSource — BSP_TS_Init/BSP_TS_GetState polling
  main.cpp                       MPU_Config/CPU_CACHE_Enable/SystemClock_Config verbatim from the
                                 example; HAL_Init; LED; one FreeRTOS task -> app::run; scheduler
  renode/
    stm32h7b3i_dk.repl            stm32h7.repl + axiSram 1 MiB + rcc hseFrequency 24 MHz +
                                 Input.FT5336 @ i2c4 0x38 (MaxX 480, MaxY 272) -> gpioPortH@2
    run.resc                     mach create, LoadPlatformDescription, LoadELF, showAnalyzer
                                 sysbus.ltdc, start
    screenshot.resc              headless: RunFor "5", TakeScreenshot().Save(png) via python, quit
app/
  demo.hpp                       DemoModel, hand-written view(): title, counter, "+1" button,
                                 checkbox, slider
  runtime.hpp/.cpp               run(FrameSink&, TouchSource&): logic thread (model+WidgetTree+
                                 touch polling+publish) and render thread (draw+present), handoff
                                 via mutex + shared_ptr<const SceneSnapshot> + condition_variable
meson.build                      hal_lib, bsp_lib, freertos/posix rebuilt per board config,
                                 stm32_board (startup .s, system_stm32h7xx.c, ...), and the targets
                                 stm32h7b3i_dk.elf, stm32h7b3i_dk_renode.elf (-DPRISM_RENODE),
                                 stm32h7b3i_dk.bin (objcopy), stm32h7b3i_dk.size.txt (arm-none-eabi-size)
README.md                        "Run it without the board" (Renode) and "Flash the board"
```

Build (controller, as before): `scripts/remote.sh 'meson setup build --cross-file
cross/stm32h7.ini -Dprism_dir=$HOME/PRSIM && ninja -C build'`.

## 2. The input path used (PRISM's public API, no PRISM changes)

`app/runtime.cpp` turns the polled touch into PRISM events with
**`prism::app::widget_detail::route_event(tree, snap.get(), ev)`**
(`include/prism/app/event_routing.hpp`) — the same top-level dispatcher `AppRun::dispatch`
uses. `ev` is in window coordinates (480x272 = panel pixels, the same coordinates the
framebuffer uses) and `snap` is the last **shown** snapshot, which `route_event` hit-tests
against (`input::hit_test`) and localizes per widget (`local_to`). Events emitted:

- finger down:  `MouseButton{pos, input::buttons::left, pressed=true}`
- finger up:    `MouseButton{pos, input::buttons::left, pressed=false}`
- finger move:  `MouseMove{pos, input::buttons::left_mask}`

The logic thread is the only one that sets model fields (`route_event` -> `dispatch` ->
the delegate's `handle_input` -> `Field::set`), which matches `core::may_change_model()`
(no `model_app` runs, so any thread may, but only the logic thread does). The "+1" button
is wired with `model.increment.on_change().connect(... counter.set(+1))`, fired from the
click during dispatch. This is the pattern `tests/test_widget_tree.cpp` and
`tests/test_model_app.cpp` exercise on the host.

## 3. Memory map (as linked by boards/stm32h7b3i_dk/stm32h7b3.ld)

| Region | Address | Size | Contents |
|---|---|---|---|
| FLASH | 0x08000000 | 2 MiB | vectors (`.isr_vector` first), code, rodata, load image of .data/.tdata |
| AXI SRAM | 0x24000000 | 1 MiB | .data/.tdata/.bss (incl. heap_4's 384 KiB), FreeRTOS task stacks, posix TLS blocks, newlib heap (`end`..`_heap_end` = top − 16 KiB), main stack at the top (0x24100000) |
| DTCM | 0x20000000 | 128 KiB | unused (not in the script) |
| SDRAM | 0xD0000000 | 16 MiB | not in the script (exists only after `BSP_SDRAM_Init`): LTDC front buffer 0xD0000000, back buffer 0xD0080000, 480*272*4 = 522240 bytes each |

`_sbrk` (sbrk.c) grows the newlib heap from `end` to `_heap_end` (= `_estack` − 16 KiB) and
returns −1/ENOMEM at the limit; it needs no lock of its own because newlib's malloc is
serialized by the retargetable locks in `posix/newlib_locks.c`. FreeRTOS heap (heap_4,
384 KiB static) holds the task stacks: app task 32 KiB, idle, timer task, and the two
pthread stacks (logic 64 KiB, render 48 KiB — the QEMU PRISM task used 64 KiB for snapshot
building) plus TLS blocks. `posix/tls.c` copies each thread's TLS from `_tdata_lma` in
flash, so the CMSIS startup not copying `.tdata` to RAM is fine (verified in
`posix/freertos_posix.c`: even the pre-scheduler boot thread gets a TLS block from the LMA).

## 4. Board bring-up decisions, and where I was unsure

- **Startup/clock/power**: the CMSIS `startup_stm32h7b3xxq.s` is used as-is (it calls
  `ExitRun0Mode` + `SystemInit`, copies .data, zeroes .bss, runs `__libc_init_array`, calls
  `main`); `system_stm32h7xx.c` from the cube-example as-is (SystemInit enables the FPU and
  sets VTOR=0x08000000). `main.cpp` copies `MPU_Config` (SDRAM write-through region),
  `CPU_CACHE_Enable` and `SystemClock_Config` (280 MHz from the 24 MHz HSE, VOS scale 0)
  **verbatim** from the example. Build defines `STM32H7B3xxQ`, `USE_HAL_DRIVER`,
  `USE_PWR_DIRECT_SMPS_SUPPLY` (from ST's .cproject; `ExitRun0Mode` then just clears LDOEN
  and waits ACTVOSRDY). Under `-DPRISM_RENODE`, `SystemClock_Config` is skipped (Renode's
  clock tree/PLL-ready flags are not faithful) and the board runs on the reset clock — HSI
  64 MHz, which is also `SystemCoreClock`'s reset value, so `configCPU_CLOCK_HZ =
  SystemCoreClock` stays consistent.
- **HAL tick**: TIM6 (template adapted; `uwTickPrio` is `extern` in stm32h7xx_hal.h —
  verified). `TICK_INT_PRIORITY 0x0F` is numerically below `configMAX_SYSCALL_INTERRUPT_PRIORITY`
  (0x50 = level 5 of the 4 implemented priority bits, `__NVIC_PRIO_BITS==4`), i.e. more
  urgent than any FromISR-legal interrupt — safe because the TIM6 handler only calls
  `HAL_IncTick()`. FreeRTOS's SysTick/PendSV/SVC are mapped onto the CMSIS vector names via
  the usual `#define xPortSysTickHandler SysTick_Handler` etc. in FreeRTOSConfig.h.
  **Unsure**: whether `LTDC_SRCR_VBR` is the exact CMSIS bit name used to poll the vblank
  reload in `present()` (standard name; if wrong it's a compile error, trivial to fix) and
  that the hardware clears SRCR.VBR when the reload is applied (RM0468 behaviour).
- **Display**: `BSP_LCD_InitEx(0, LCD_ORIENTATION_LANDSCAPE, LCD_PIXEL_FORMAT_RGB888, 480,
  272)` — read in `stm32h7b3i_discovery_lcd.c`, `LCD_PIXEL_FORMAT_RGB888` maps to
  `LTDC_PIXEL_FORMAT_ARGB8888` (4 B/px, matching `raster::Canvas`), layer 0 window 480x272
  at `LCD_LAYER_0_ADDRESS` = 0xD0000000. `BSP_LCD_InitEx` asserts the backlight/display-enable
  pins and starts the TIM2 PWM brightness; `BSP_LCD_DisplayOn(0)` is called after it.
  **Unsure**: the SDRAM refresh count (`is42s16800j_conf.h`) — I computed 1438 from
  SDCLK = HCLK/3 = 93.3 MHz (the BSP's own `FMC_SDRAM_CLOCK_PERIOD_3` and its comment), but
  ST's shipped value for this exact board is not in the repo to compare against; a wrong
  (too large) count shows as SDRAM data corruption, not a boot failure. Also unsure whether
  `BSP_LCD_InitEx`'s internal second `BSP_SDRAM_Init(0)` (we call it first ourselves for a
  distinct error path) is harmless — it re-initializes the FMC bank before any SDRAM
  access, which should be; and whether the backlight PWM default brightness is visible
  (TIM2 PWM at ~50% per the BSP). `present()`: `SCB_CleanDCache_by_Addr` on the back buffer
  (redundant — MPU region 1 is write-through — but kept, as instructed; both buffers are
  32-byte aligned and 522240 is a multiple of 32), `HAL_LTDC_SetAddress_NoReload(&hlcd_ltdc,
  back, 0)`, `HAL_LTDC_Reload(LTDC_RELOAD_VERTICAL_BLANKING)`, poll `LTDC->SRCR &
  LTDC_SRCR_VBR` clearing with a 100 ms `HAL_GetTick` timeout, swap front/back.
- **Touch**: `BSP_TS_Init(0, {480, 272, TS_SWAP_NONE, accuracy 2})`. The BSP probes GT911
  first (I2C4 0xBA) — on this board that read fails (polled I2C, 1000 ms timeout per
  transfer in `stm32h7b3i_discovery_bus.c`, so boot may take ~1 s extra) and the FT5336
  (0x70 = 7-bit 0x38) answers. `BSP_TS_GetState` scales to 480x272 and applies the
  orientation. **Unsure (biggest hardware unknown)**: the touch axis orientation —
  `TS_SWAP_NONE` with a 480x272 FT5336 geometry assumes the controller reports in panel
  landscape axes. If on the real board (or in Renode) the axes come out swapped/mirrored,
  flip the `TS_SWAP_XY`/`TS_SWAP_X`/`TS_SWAP_Y` bits in `touch.cpp`'s `TS_Init_t` (and
  `isRotated` on the Renode FT5336). `BSP_GetTick` is `HAL_GetTick()` (verified in bus.c).
- **LED**: `BSP_LED_Init(LED2)` (PG2, blue) in `main`, toggled from `vApplicationIdleHook`
  every 250 ms of HAL tick — alive even with a black screen.
- **Widget sizes**: ViewBuilder has no per-leaf size hints (only `CanvasHandle.min_size` /
  `TextHandle.rows`), so the demo relies on the delegates' default 30 px row height plus
  explicit `vb.gap()`s — ~150 px of content on the 272 px panel. If too small for a finger,
  that's the knob to turn (there is no larger default).
- **meson**: the CMSIS startup `.s` is listed as a source of `stm32h7b3i_dk_board` — meson
  assembles `.s/.S` with the C compiler driver (`-x assembler-with-cpp`), the same way the
  existing build already compiles `posix/tls_support.S` in a `c,cpp`-only project. If meson
  ever rejects it, wrap it in a `custom_target` running the c compiler with `-x
  assembler-with-cpp -c`. FreeRTOS and posix/ are built twice (own `FreeRTOSConfig.h` per
  board) so the two configs never mix; `posix` is `link_whole` (gthr-posix references the
  pthread_* weakly; newlib's retarget locks must precede libc.a's `libc_a-lock.o`).
- **`app/runtime.cpp` shape**: `run()` creates the two threads with `pthread_create`
  (64/48 KiB stacks, from heap_4) because `std::thread` can't set a stack size. The handoff
  is `mutex + condition_variable + shared_ptr<const SceneSnapshot>` (`pending` to draw,
  `shown` for input hit-testing — the same `shown` model_app dispatches against), not
  `core::atomic_cell`, because input needs the *displayed* frame, not the latest.
  `raster::Fonts` is shared by both threads (its header documents the thread-safety:
  immutable faces, mutex-guarded glyph cache). No logging on the board (no UART set up);
  `printf` would go to libnosys's no-op `_write`.

## 5. Renode expectations and gaps

Platform `boards/stm32h7b3i_dk/renode/stm32h7b3i_dk.repl` = `platforms/cpus/stm32h7.repl`
+ AXI SRAM 1 MiB (repl default 512 KiB), `rcc hseFrequency 24 MHz`, and
`Input.FT5336 @ i2c4 0x38 { MaxX: 480, MaxY: 272, -> gpioPortH@2 }` (PH2 = TS_INT → EXTI2,
which the cpu repl wires to nvic@8 = EXTI2_IRQn). Flash needs no override: the repl's two
1 MiB banks (0x08000000/0x08100000) already give 2 MiB behind the flash controller.

Already covered by the cpu repl: PWR (Python stub, ACTVOSRDY/VOSRDY set — `ExitRun0Mode`
and the VOSRDY wait pass), RCC (modeled), FMC (Python stub absorbing the SDRAM register
writes; the framebuffers are plain memory in `sdramBank2` 0xD0000000, so reads/writes work),
LTDC (`Video.STM32LTDC` at 0x50001000, `showAnalyzer sysbus.ltdc`), TIM6, I2C4, DMA2D, GPIO,
EXTI, SYSCFG, flash controller.

Expected gaps / things for the controller to check:

1. **Clock**: `-DPRISM_RENODE` skips `SystemClock_Config`, so the emulated CPU stays on the
   reset clock. What frequency Renode's CortexM actually runs at after reset (HSI 64 MHz?)
   is not something I could verify from the repl — `configCPU_CLOCK_HZ = SystemCoreClock`
   (64 MHz) keeps FreeRTOS's SysTick self-consistent with the *variable*, but if the
   emulated CPU clock differs, the tick just runs at a different virtual rate (delays
   still work). The repl's timers are fixed at 250 MHz, so TIM6's 1 kHz HAL tick in virtual
   time is whatever 250 MHz/(presc+1)/(period+1) gives — HAL timeouts (I2C 1000 ms, LTDC
   reload 100 ms) may be shorter in virtual time than on the board.
2. **Screenshot API**: `screenshot.resc` uses
   `python "img = monitor.Machine['sysbus.ltdc'].TakeScreenshot(); img.Save('$out')"` —
   `TakeScreenshot` returning `RawImageData` with a `Save(path)` (PNG) is my recollection of
   Renode's video peripherals, **not verified in this install** (no grep available). If the
   names differ, `help sysbus.ltdc` in the monitor lists the methods; the `monitor.Machine[... ]`
   accessor pattern is verified from `scripts/monitor.py`/`tests/platforms/echo-i2c-peripheral.py`.
3. **Touch injection headless**: the FT5336 model's method names are unknown to me —
   `help sysbus.touchscreen` in the monitor lists them; `run.resc`'s GUI (mouse on the ltdc
   analyzer) is expected to drive it (the f7_discovery-bb platform attaches the same model).
   If the emulated touch axes are swapped vs. the framebuffer, set `isRotated: true` on the
   touchscreen (and/or flip the TS_SWAP_* bits in the firmware).
4. **GT911 probe first**: on I2C4 at 0xBA nothing answers in Renode, so the probe should
   NACK quickly; if the I2C model stalls, `BSP_TS_Init` could hang on its 1000 ms timeouts
   (in virtual time, possibly instantly).
5. **MPU/caches**: Renode's CortexM has 16 MPU regions; `MPU_Config`/`SCB_EnableDCache`
   writes are absorbed. D-cache clean in `present()` is a no-op there (WT anyway).
6. `emulation RunFor "5"` runs 5 virtual seconds — with the reset clock the app's 60 Hz
   polling loop may have run thousands of frames; that's fine, the screenshot is the last
   presented frame.

## 6. Flashing (README has the commands)

`st-flash --reset write build/stm32h7b3i_dk.bin 0x08000000` — **unsure** whether stlink 1.8
fully supports the H7B3's flash programming (recent stlink handles H7; if not, the OpenOCD
alternative `openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program
build/stm32h7b3i_dk.elf verify reset exit"` is in the README). Expect: LED2 blinking ~2 Hz,
the LCD showing the demo, touch working. If it doesn't boot: connect under reset, check
`build/stm32h7b3i_dk.size.txt` against the 1 MiB AXI SRAM, and remember the power supply
must be direct SMPS (`USE_PWR_DIRECT_SMPS_SUPPLY` is defined) — power-cycle to clear a lock.
