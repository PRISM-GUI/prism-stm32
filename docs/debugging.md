# Flash, log and debug

## Flash

Images are raw binaries at 0x08000000 (the vector table sits at the start of flash):

    scripts/fetch-firmware.sh stm32f746g_disco build-f7
    st-flash --reset write build-local/stm32f746g_disco.bin 0x08000000

    scripts/fetch-firmware.sh stm32h7b3i_dk build
    st-flash --reset write build-local/stm32h7b3i_dk.bin 0x08000000

With `scripts/fetch-firmware.sh <board> <build-dir>` the images land in `build-local/`; the script
copies the ELF, the bin and the Renode ELF.

`st-flash` 1.8 may not perform the H7B3's flash unlock sequence. If it refuses, program the ELF with
OpenOCD instead:

    openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
        -c "program build-local/stm32h7b3i_dk.elf verify reset exit"

For the F7 the target file is `target/stm32f7x.cfg`.

## The serial log

Both boards talk to the host through the ST-LINK's virtual COM port — `/dev/ttyACM0` on Linux,
`/dev/tty.usbmodem*` on macOS. **The F7 uses 115200 baud 8N1, the H7's profile dump 921600.**

    picocom -b 115200 /dev/ttyACM0        # F7
    prism-trace /dev/ttyACM0 -o trace.json --baud 921600   # H7 profile dump

### What a healthy F7 boot prints

`board_early_init()` writes to USART1 before HAL, before the clock switch and before the RTOS, so
the log starts at the reset clock rate and continues at 115200 once the clocks are set:

    f746: boot
    f746: mpu+cache
    f746: hal
    f746: clocks
    f746: sdram ok (8 MB)
    f746: tasks
    f746: lcd clock
    f746: lcd pins
    f746: ltdc
    f746: display ready
    f746: touch
    f746: app

Read it as a checklist: the last line printed names the step that failed.

- No line at all: the chip is not running (power, reset, or the wrong clock setup before the first
  write).
- `f746: sdram init failed`, or `f746: sdram FAIL at <addr> wrote <word> read <word>` followed by
  `f746: sdram bad words <n>`: the external SDRAM is not working. The test writes a pattern through
  all 8 MiB, cleans and invalidates the cache, and reads it back, so it covers the wiring and the
  controller timings, not just the first word.
- Stops after `f746: lcd` lines: the panel or the pixel clock, not the heap.
- `f746: no touch`: the FT5336 did not answer; the demo still runs, without input.

### The fault report

A HardFault on the F7 prints and then parks:

    !! fault in task app
    cfsr 00000000 hfsr 40000000 mmfar e000ed34 bfar e000ed38
    pc 0800abcd lr 08001234 psr 61000000 r0 00000000 r1 20001f00 r2 00000004 r3 00000000

`cfsr` (0xE000ED28) says what faulted, `hfsr` (0xE000ED2C) whether it was escalated, `mmfar` and
`bfar` (0xE000ED34/38) the address for memory-management and bus faults. The `pc` is the faulting
instruction; feed it to the toolchain:

    arm-none-eabi-addr2line -f -C -i -e build-f7/stm32f746g_disco.elf 0x0800abcc

Clear the Thumb bit (the low bit) before the lookup if the address ends in an odd digit.

`prism fault` does the whole report at once, `pc` and `lr` included:

    prism fault build-local/stm32f746g_disco.elf crash.txt

It prints one line per address, with the function and the `file:line`. With no file it reads the
report from stdin, so the serial log can be piped in. It runs `arm-none-eabi-addr2line` from
`PATH`, and prints `??` for an address that is not in that ELF — the usual reason is the wrong
image. `prism` comes from PRISM's `tools/prism-cli/`; its page is
https://prism-gui.readthedocs.io/en/latest/start/tooling.html

## Debugging with OpenOCD

Start the server, then talk to it over telnet (4444) or with `-c` commands in one shot:

    openocd -f interface/stlink.cfg -f target/stm32f7x.cfg        # or stm32h7x.cfg

    openocd -f interface/stlink.cfg -f target/stm32f7x.cfg \
        -c "init; reset halt; reg pc; mdw 0x08000000 4; shutdown"

Useful commands over telnet:

| command | what it gives you |
|---|---|
| `reset halt` | stop the core at the reset vector, before any code runs |
| `reg` | every core register, including `xpsr`, `msp`, `psp` |
| `reg pc`, `reg lr`, `reg r0` | one register |
| `mdw <addr> <count>` | words of memory (e.g. a framebuffer, or the vector table) |
| `mdh`, `mdb` | half-words and bytes |
| `step`, `resume`, `halt` | execution control |
| `rtt` / `arm semihosting` | only if the firmware uses them — it does not |

Connect **under reset** when a board does not boot: `reset halt` right after `init`, or hold the
NRST pin while `st-flash` starts. A board that has taken a bad clock or power configuration can
look dead until it is halted that way.

If the screen is black but the log is complete, the heap is the usual suspect: check
`build-f7/stm32f746g_disco.size.txt` (the FreeRTOS pool and the C heap must fit the SDRAM, which
the linker script sizes) and then watch the display: `mdw 0xC0000000 16` shows whether anything
was written to the front framebuffer.

## Running under Renode

Renode runs the `*_renode.elf` image. It is the same firmware with `-DPRISM_RENODE`, which:

- skips the F7's clock setup (the PWR model has no over-drive handshake) and the SDRAM init (there
  is no FMC model; the SDRAM is plain mapped memory),
- narrows the LTDC active width to the layer, because the emulated LTDC uses the active width as
  its line length (ST's timings give a 491-pixel active area around a 480-pixel layer),
- clears damaged rectangles on the CPU instead of the DMA2D, and polls the LTDC reload flag instead
  of waiting for its interrupt.

From `boards/<board>/renode/` (the `.repl` path resolves against the working directory):

    PRISM_SCREENSHOT=/tmp/shot.png ~/opt/renode/renode --disable-gui --console \
        -e '$bin = @/abs/path/to/stm32f746g_disco_renode.elf' screenshot.resc

Scripts per board: `run.resc` (GUI), `screenshot.resc`, `tap.resc` (two taps on "+1"), `widgets.resc`
(checkbox and slider). The emulated panel is an FT5336; `move`/`press`/`release` on
`sysbus.i2c3.touchscreen` (F7) or `sysbus.i2c4.touchscreen` (H7) drive it.

What Renode does not reproduce: caches, the SDRAM/FMC controller and its timings, the DMA2D, the
ADC, the DWT on the F7 platform, and the FPU precision rule. A bug that needs one of those — a
missing cache clean, an SDRAM timing, a double-precision instruction on the F7 — passes in Renode
and fails on the board.

## Where the bytes went

After a build, `<board>.size.txt` has `text`, `data` and `bss`. Two scripts and `prism size` turn a
linker map into a report:

    cd build-f7 && ninja -t commands stm32f746g_disco.elf | tail -1 > /tmp/link.sh
    sed -i 's|$| -Wl,-S -Wl,--cref -Wl,-Map=/tmp/map.txt|' /tmp/link.sh
    sh /tmp/link.sh
    python3 ../scripts/size-map.py /tmp/map.txt          # bytes per object, grouped
    python3 ../scripts/size-sections.py /tmp/map.txt     # biggest input sections, with who pulls them
    prism size /tmp/map.txt --top 5                      # flash by origin, then RAM, then the biggest

`-Wl,-S` matters for the two `size-*.py` scripts: without it the map lists debug sections and their
totals come out several times too large.

`prism size` reads the map's `Memory Configuration` table instead, so a `.bss` or the heap is
counted as RAM, never as flash, and on the F746 widgets build its flash total is within a kilobyte
of the `.bin`. It names the family behind each line (`prism`, `prism:ui`, `app`, `libstdc++`,
`fonts`, `freertos`, `hal`, …). It comes from PRISM's `tools/prism-cli/`; its page is
https://prism-gui.readthedocs.io/en/latest/start/tooling.html
