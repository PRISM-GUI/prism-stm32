# Audio Scope Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A live audio scope on the STM32H7B3I-DK (microphones → waveform and spectrum plots in
PRISM, with a timing readout) that runs the same way in Renode and on the host.

**Architecture:** The runtime (`app/runtime.cpp`) stops hard-coding the widget demo and runs an
`app::Example` (the widget demo or the scope). The scope's pure parts (spectrum, synthetic
signal, model update, timing aggregation) are host-tested; the microphone source is the only
board-only part. Timing is measured in the runtime around snapshot build, draw and present.

**Tech Stack:** C++26 (GCC 15 cross / GCC 16 host), PRISM `plot::PlotModel`, CMSIS-DSP
`arm_rfft_fast_f32`, ST BSP audio-in (DFSDM, instance 2), FreeRTOS, meson, doctest, Renode 1.17.

**Spec:** docs/superpowers/specs/2026-11-20-audio-scope-design.md

## Global Constraints

- PRISM (`prism_dir`) is read-only. A needed PRISM change is reported, not made.
- Fields are set on the logic thread only. The audio DMA callback (interrupt) only hands a
  finished block over; FFT and model updates run on the logic thread.
- Plots go through PRISM: `plot::PlotModel` + `vb.canvas(plot).depends_on(...)` as in
  PRISM's `examples/showcase/showcase_plot.cpp`. No drawing outside PRISM.
- No optimization of PRISM in this work: build and measure.
- Builds on Coder via `scripts/remote.sh '<cmd>'`, one build at a time, foreground; firmware is
  fetched with `scripts/fetch-firmware.sh`; host tests: `scripts/remote.sh 'meson test -C build-host'`
  (check README for the exact host build dir and adapt).
- Screen 480x272; touch targets at least 40 px tall.
- Style: PRISM's AGENTS.md (KISS, small functions, strong geometry types at APIs, `simplify:`
  comments on cuts). Commit with explicit pathspecs.

## Decisions (made here, binding on tasks)

- Sample rate 16 kHz, FFT size 1024, Hann window, mono = microphone 1 only (`simplify:` mixing
  both is a later choice).
- Spectrum: magnitudes of bins 0..511, normalized so a full-scale sine reads 0 dB; dB floor
  −100 dB. Linear scale = the same magnitudes, 0..1.
- Waveform shows the last 320 samples (20 ms).
- FFT: CMSIS-DSP on both host and board (one code path; its portable C builds on the host).
  Cloned to `third_party/CMSIS-DSP` at tag `v1.16.2`, documented in the README's dependency list.
- Example choice: meson option `example` (combo `scope`, `widgets`; default `scope`), applied to
  both `stm32h7b3i_dk.elf` and `stm32h7b3i_dk_renode.elf`.
- The board firmware falls back to the synthetic source when microphone init fails; the title
  says which source runs ("mic" or "synthetic").

## Review Focus

- Silence (all-zero samples): spectrum stays finite at the −100 dB floor, no NaN/−inf. → Task 1 test.
- Loud input at maximum gain (clipping): values stay finite, waveform clamped to ±1. → Task 3 test.
- Fewer samples than the FFT size at startup: no crash, spectrum computed on the zero-padded window. → Task 3 test.
- Freeze then unfreeze: samples that arrived while frozen are dropped, no burst of stale data. → Task 3 test.
- A second with no frames (frozen, nothing dirty): timing shows 0 fps, not the previous numbers. → Task 2 test.

---

### Task 1: Spectrum and synthetic signal (pure, host-tested)

**Files:**
- Create: `app/spectrum.hpp`, `app/spectrum.cpp`, `app/synthetic_source.hpp`, `app/synthetic_source.cpp`, `app/sample_source.hpp`
- Modify: `host/meson.build` (CMSIS-DSP static library from `third_party/CMSIS-DSP`, new tests), `meson.build` (same library for the cross build), `README.md` (CMSIS-DSP clone line)
- Test: `host/tests/test_spectrum.cpp`, `host/tests/test_synthetic_source.cpp`

**Interfaces:**
- Produces:
  - `struct app::SampleSource { virtual std::size_t read(std::span<float> out) = 0; virtual float sample_rate() const = 0; virtual const char *name() const = 0; virtual ~SampleSource() = default; };` — non-blocking: copies up to `out.size()` available samples in [−1, 1], returns the count.
  - `class app::Spectrum { public: explicit Spectrum(std::size_t fft_size); void compute(std::span<const float> window_in, std::span<float> magnitudes_db); void compute_linear(std::span<const float> window_in, std::span<float> magnitudes); };` — `window_in.size() == fft_size`, outputs `fft_size / 2`.
  - `class app::SyntheticSource final : public SampleSource` — sine sweep 200 Hz → 4 kHz over 5 s, repeating, amplitude 0.5, plus noise at amplitude 0.01; `explicit SyntheticSource(float sample_rate)`; time advances by real elapsed time (`steady_clock`), capped at 1 s of samples per `read`.

- [ ] **Step 1: Clone CMSIS-DSP and build it on the host**

`git clone --depth 1 --branch v1.16.2 https://github.com/ARM-software/CMSIS-DSP third_party/CMSIS-DSP` (locally, then sync). Static library from the aggregate sources `Source/TransformFunctions/TransformFunctions.c`, `Source/CommonTables/CommonTables.c` and whatever else `arm_rfft_fast_f32` needs to link; include `Include/` and `PrivateInclude/`. On the host, `arm_math.h` needs a CMSIS core header: use `third_party/cmsis-core` if that works, otherwise say what you did.

- [ ] **Step 2: Write the failing tests**

```cpp
TEST_CASE("spectrum: a 1 kHz sine peaks in bin 64") {          // 1000 / (16000/1024) = 64
    app::Spectrum s{1024};
    std::vector<float> in(1024), db(512);
    for (std::size_t i = 0; i < in.size(); ++i) in[i] = std::sin(2 * pi * 1000.f * i / 16000.f);
    s.compute(in, db);
    CHECK(std::distance(db.begin(), std::ranges::max_element(db)) == 64);
    CHECK(std::abs(db[64]) < 0.5f);                               // full scale ≈ 0 dB
}
TEST_CASE("spectrum: silence stays at the floor") {
    // all zeros → every bin == -100 dB, all finite
}
TEST_CASE("spectrum: linear magnitudes of a full-scale sine peak near 1") { /* bin 64 ≈ 1 ±0.05 */ }
TEST_CASE("synthetic source: reads at most what elapsed, values in [-1, 1]") {}
TEST_CASE("synthetic source: dominant frequency moves up over the sweep") {
    // feed two 1024-sample blocks taken 1 s apart (inject time: give SyntheticSource a
    // `read_at(std::span<float>, double t_seconds)` used by read(); test through it)
}
```

- [ ] **Step 3: Run them; they fail to compile** — `scripts/remote.sh 'meson test -C build-host test_spectrum test_synthetic_source'` (adapt dir).

- [ ] **Step 4: Implement** `Spectrum` (Hann window, `arm_rfft_fast_f32`, magnitude = |X| · 2 / Σwindow, dB = max(20·log10(mag), −100)) and `SyntheticSource`.

- [ ] **Step 5: Run the full host suite; all pass.** Then cross-build `ninja -C build` and confirm the firmware still links with the CMSIS-DSP library added to `meson.build` (not yet used by the firmware: just built).

- [ ] **Step 6: Commit** (`app/spectrum.*`, `app/synthetic_source.*`, `app/sample_source.hpp`, `host/meson.build`, `host/tests/test_spectrum.cpp`, `host/tests/test_synthetic_source.cpp`, `meson.build`, `README.md`).

### Task 2: Runtime runs an `Example`, and measures itself

**Files:**
- Create: `app/example.hpp`, `app/frame_stats.hpp`, `app/frame_stats.cpp`, `app/widgets_example.hpp` (today's `DemoModel` wrapped as an Example)
- Modify: `app/runtime.hpp`, `app/runtime.cpp`, `app/demo.hpp` (only if the wrapper needs it), `boards/stm32h7b3i_dk/main.cpp`, `meson.build`, `meson.options` (`example` option)
- Test: `host/tests/test_frame_stats.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces:
  - `struct app::StageTimes { std::chrono::microseconds snapshot, draw, present; };`
  - `class app::FrameStats { public: void add(StageTimes); /* one finished frame */ std::optional<std::string> tick(std::chrono::steady_clock::time_point now); };` — `tick` returns a new line once per second, format exactly `"{fps} fps · snapshot {a} ms · draw {b} ms · present {c} ms"` with fps an integer and the ms values averages with one decimal; a second with no frames gives `"0 fps · snapshot - · draw - · present -"`; it resets after each line.
  - `struct app::Example { virtual prism::app::WidgetTree &tree() = 0; /* built on the logic thread */ virtual void tick() = 0; /* each logic loop, before the dirty check */ virtual void show_stats(const std::string &line) = 0; virtual ~Example() = default; };`
  - `using app::ExampleFactory = std::unique_ptr<Example> (*)();` — called on the logic thread.
  - `void app::run(FrameSink &, TouchSource &, ExampleFactory)` replaces `run(FrameSink&, TouchSource&)`.
  - `std::unique_ptr<Example> app::make_widgets_example();`

- [ ] **Step 1: Write the failing test** `test_frame_stats.cpp`: two frames (snapshot 2 ms/4 ms, draw 10/20, present 1/1) then `tick(start + 1 s)` → `"2 fps · snapshot 3.0 ms · draw 15.0 ms · present 1.0 ms"`; a following `tick(start + 2 s)` with no frames → `"0 fps · snapshot - · draw - · present -"`; `tick(start + 0.5 s)` → `nullopt`.
- [ ] **Step 2: Run; fails to compile.**
- [ ] **Step 3: Implement `FrameStats`; run; passes.**
- [ ] **Step 4: Rework the runtime.** The logic loop: route touch, `example.tick()`, build the snapshot if dirty (timed), hand it over. The render thread times draw and present and passes the snapshot's build time through with it (put `snapshot` time next to `pending` in `Handoff`); the finished `StageTimes` go back to the logic thread through `Handoff` (a small vector under the existing mutex). The logic thread feeds `FrameStats` and calls `show_stats` with each line. The widgets example ignores `show_stats` (`simplify:` comment).
- [ ] **Step 5: `main.cpp` passes the factory chosen by the `example` option** (a define per target from meson). Build both firmware targets; run `tap.resc` in Renode with `-Dexample=widgets`: counter shows 2 (same as before this task).
- [ ] **Step 6: Commit.**

### Task 3: The scope model and view (host-tested)

**Files:**
- Create: `app/audio_scope.hpp`, `app/audio_scope.cpp`
- Modify: `host/meson.build`, `meson.build`
- Test: `host/tests/test_audio_scope.cpp`

**Interfaces:**
- Consumes: `SampleSource`, `Spectrum` (Task 1); `Example`, `show_stats` (Task 2).
- Produces:
  - `struct app::ScopeModel { plot::PlotModel waveform, spectrum; core::Field<bool> frozen{false}; core::Field<float> gain{1.f}; /* slider 0.5..8 */ core::Field<bool> db_scale{true}; core::Field<std::string> title, stats; void view(prism::app::ViewBuilder &); };`
  - `class app::AudioScope final : public Example { public: explicit AudioScope(SampleSource &); void tick() override; /* drain source, update plots */ ... };` and `std::unique_ptr<Example> app::make_scope_example(SampleSource &)` (the factory in `main.cpp` wraps it with the board's source).
  - `tick()`: reads all available samples into a rolling 1024-sample window (zeros at start); when frozen, reads and drops them; when not frozen and at least one new sample arrived: waveform series = last 320 samples × gain, clamped to ±1; spectrum series = `Spectrum` of the window × gain, dB or linear per `db_scale`; y ranges fixed: waveform [−1, 1], spectrum [−100, 0] dB or [0, 1] linear; x: waveform 0..20 ms, spectrum 0..8000 Hz. `title` = `"PRISM audio scope · " + source.name()`.

- [ ] **Step 1: Write the failing tests** with a fake `SampleSource` that returns queued samples:
  - one tick with 1024 samples of a 1 kHz sine: spectrum series has 512 points, its maximum at x ≈ 1000 Hz (±16); waveform series has 320 points.
  - startup with 100 samples: no crash, both series populated (zero-padded window).
  - gain 8 on a full-scale sine: every waveform y in [−1, 1], every spectrum y finite.
  - frozen: a tick with new samples leaves both series unchanged; after unfreezing, a tick with 1024 new samples of 2 kHz shows the peak at 2 kHz, with nothing from the dropped samples.
  - `build_snapshot(480, 272, 1)` of a `WidgetTree` on the model succeeds and is not empty; the stats label shows the string given to `show_stats`.
- [ ] **Step 2: Run; fail.** **Step 3: Implement.** Plot update: use the PlotModel API the way PRISM's examples do (`clear_series` + `add_series` + `notify`/`revision`, or an in-place source if PRISM's `perf_lab` does that); write which one and why in a comment.
- [ ] **Step 4: Run the full host suite; passes.**
- [ ] **Step 5: Host benchmark** `host/scope_bench.cpp`: AudioScope on `SyntheticSource`, 300 frames of tick + build_snapshot + raster draw into a 480x272 canvas, prints the FrameStats line(s) and writes `scope.ppm`. Run it, look at the image.
- [ ] **Step 6: Commit.**

### Task 4: Board microphones and the firmware

**Files:**
- Create: `boards/stm32h7b3i_dk/mic_source.hpp`, `boards/stm32h7b3i_dk/mic_source.cpp`
- Modify: `boards/stm32h7b3i_dk/main.cpp`, `boards/stm32h7b3i_dk/stm32h7xx_hal_conf.h` (DFSDM, DMA modules), `boards/stm32h7b3i_dk/stm32h7xx_it.c` (the DFSDM DMA IRQ handler the BSP expects), `meson.build` (BSP audio source, HAL DFSDM sources), `README.md`, `boards/stm32h7b3i_dk/renode/screenshot.resc` if the scope needs a longer run
- Test: Renode screenshot; the user's board run

**Interfaces:**
- Consumes: `SampleSource` (Task 1), `make_scope_example` (Task 3).
- Produces: `class MicSource final : public app::SampleSource { public: bool init(); /* BSP_AUDIO_IN_Init(2, …) at 16 kHz, mic 1, then record */ ... };`

- [ ] **Step 1:** Read ST's audio-record example for this board (STM32CubeH7 `Projects/STM32H7B3I-DK/Examples/BSP/Src/audio_record.c` or the DFSDM one) for the init parameters, buffer format, and the half/complete transfer callbacks.
- [ ] **Step 2:** Implement `MicSource`: DMA double buffer; the half/complete callbacks convert the finished half to float in [−1, 1] and push it into a single-producer single-consumer ring (interrupt → logic thread; PRISM's `core` may have one: reuse it if it is ISR-safe, otherwise a small lock-free ring with a `simplify:` comment); on overflow drop the oldest. D-cache: invalidate the DMA half before reading it (the buffer is in AXI SRAM).
- [ ] **Step 3:** `main.cpp`: `-Dexample=scope` uses `MicSource` when `init()` succeeds, `SyntheticSource{16000}` otherwise; the Renode target always uses `SyntheticSource`.
- [ ] **Step 4:** Build both targets; run `screenshot.resc` in Renode (enough seconds for a few FrameStats lines): the screenshot shows both plots, "synthetic" in the title and a stats line. Look at it.
- [ ] **Step 5:** README: what the scope shows, how to pick the example, what to expect on the board.
- [ ] **Step 6: Commit.** Then the user flashes the board and reports the stats line and whether whistling moves the spectrum peak.
