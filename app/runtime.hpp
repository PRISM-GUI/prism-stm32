#pragma once

// The board-independent runtime (app/): run(FrameSink&, TouchSource&, ExampleFactory) drives
// an Example (app/example.hpp) on two threads — a logic thread that owns the Example and its
// app::WidgetTree, polls the touch at ~60 Hz and routes it through the tree's public input
// path, and a render thread that draws each new snapshot into the framebuffer and presents
// it. Each frame is timed (snapshot, draw, present) and summarized once a second
// (app/frame_stats.hpp) to Example::show_stats.
// Board code implements the two small interfaces below (boards/stm32h7b3i_dk/display.*
// and touch.*).

#include "app/example.hpp"

#include <prism/core/types.hpp>

#include <cstdint>
#include <optional>
#include <span>

namespace app {

// What the render thread draws into and presents (the board: the LTDC back buffer in SDRAM).
struct FrameSink {
    virtual std::span<std::uint32_t> back() = 0; // ARGB8888 pixels, stride = width
    virtual void present() = 0;                  // flip to what was drawn into back()
    virtual ~FrameSink() = default;
};

// The touch panel, polled: the finger's position in window/panel pixels, or nullopt
// when no finger is down.
struct TouchSource {
    virtual std::optional<prism::core::Point> read() = 0;
    virtual ~TouchSource() = default;
};

// Runs the Example the factory makes. Never returns (the threads loop forever); call it from
// a FreeRTOS task and let the task block in it.
void run(FrameSink &sink, TouchSource &touch, ExampleFactory make_example);

} // namespace app
