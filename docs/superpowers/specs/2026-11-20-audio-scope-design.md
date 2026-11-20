# Audio scope: a PRISM showcase that drives embedded optimization

## Intent

The board now runs a small widget demo with working touch. The next example is a live audio
scope: the board's microphones, a waveform plot and a spectrum plot (FFT), updated in real time.

It has two jobs:

1. **Showcase.** Show what PRISM does on a microcontroller: real data, real plots, real input.
2. **Optimization driver.** It is concrete, realistic code whose slow parts are PRISM's own
   costs (model updates, snapshot building, the plot widget, the rasterizer). We measure them on
   the board and optimize PRISM for embedded targets against it.

The second job decides most of the design: the example uses PRISM the normal way, and it
measures itself.

## Goals

- Two digital MEMS microphones (DFSDM, ST BSP audio-in instance 2) stream samples in the
  background by DMA.
- A waveform plot (the last ~20 ms) and a spectrum plot (0 to half the sample rate), both with
  PRISM's `plot::PlotModel` and `vb.canvas(plot)`, as in PRISM's `examples/showcase/showcase_plot.cpp`.
- Controls: freeze (stop updating the plots), gain (slider), spectrum scale (dB or linear).
- A timing line on screen, updated about once a second: frames per second, and per frame the
  time for the model update + snapshot build (logic thread), the draw (render thread, rasterizer)
  and the present (flip wait).
- A synthetic test signal (a slow frequency sweep plus a little noise) behind the same interface
  as the microphones, so the example runs in Renode and in the host build.
- The same numbers are also printable in the host build (`host/`), to compare host and board.

## Non-goals

- Optimizing PRISM in this work. This work builds the example and the measurement; optimization
  is the follow-up it enables. (Cheap fixes needed just to make it run are fine.)
- Audio output, recording to storage, the line-in jack, the Arduino analog pins (a later second
  input behind the same interface).
- Triggering, cursors, zoom by touch on the plots.

## Anti-goals

- Drawing the plots outside PRISM (straight into the framebuffer, a custom fast path). It would
  look smoother and hide exactly the costs we want to find.
- Changing PRISM to make the demo look good before we have measured it.

## Constraints

- prism-stm32 repo; PRISM stays a read-only dependency (`prism_dir`). PRISM changes, if any are
  found necessary, are separate commits in PRISM, proposed to the user first.
- Same threading as today (`app/runtime.cpp`): a logic thread owns the model and the widget tree;
  a render thread draws the latest snapshot. Fields are set on the logic thread only.
- The audio DMA callback runs in an interrupt: it only hands a finished block over (a lock-free
  queue or a FreeRTOS primitive); FFT and model updates run on the logic thread (or a third,
  audio thread if the FFT turns out heavy: measure first).
- FFT: CMSIS-DSP (`arm_rfft_fast_f32`) on the board. The host build needs the same FFT: build
  CMSIS-DSP's portable C for the host too, or a small own FFT used on both: builder's call,
  with the reason written down.
- Builds on Coder, one at a time; Renode for the screen and touch checks; the user flashes the
  board.
- 480x272 screen; widgets big enough for a finger.

## Approaches considered

- **A. Microphones + PRISM plot widget + timing readout** (chosen by the user). Anyone can whistle
  at it; it uses PRISM as-is, so its costs are PRISM's costs.
- **B. Arduino analog pins (ADC).** Closer to a lab tool, but needs a signal source wired in. Can
  be added later behind the same input interface.
- **C. Custom plot drawn straight into the framebuffer.** Fast, but bypasses PRISM's drawing
  path: works against the reason for the example.

## Design

- `app/audio_scope.hpp`: the model (two `PlotModel`s, freeze/gain/scale fields, a timing label
  field) and its hand-written `view()`. Board-independent.
- An input interface, like `TouchSource`: `SampleSource` gives blocks of mono float samples at a
  known sample rate. Two implementations: the DFSDM microphones (board) and the synthetic sweep
  (Renode, host).
- A pure function from a sample block to spectrum magnitudes (window, FFT, magnitude, optional
  dB), unit-tested on the host with a known sine (peak in the right bin).
- Timing: steady_clock around the three stages; the logic thread aggregates and sets the timing
  field once a second. The render thread's numbers reach the logic thread through the existing
  handoff (a small struct of atomics is enough).
- `boards/stm32h7b3i_dk/main.cpp` picks the example (the widget demo or the scope) at build time:
  two firmware targets, or a meson option. Builder's call.

## Left to the builder

- Sample rate (16 kHz is the natural choice: 8 kHz spectrum span, small blocks), FFT size
  (512 or 1024), window (Hann), update rate of the plots (every block or capped at the frame rate).
- Mono mix of the two microphones or one microphone.
- Exact layout of the controls.
- Whether the plot data is replaced (new `XYData` per frame) or updated in place, as long as the
  choice is the one PRISM's plot API intends; note the cost either way, it is a measurement.
