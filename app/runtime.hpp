#pragma once

// The board-independent demo runtime (app/): run(FrameSink&, TouchSource&) drives the
// DemoModel on two threads — a logic thread that owns the model and the app::WidgetTree,
// polls the touch at ~60 Hz and routes it through the tree's public input path, and a
// render thread that draws each new snapshot into the framebuffer and presents it.
// Board code implements the two small interfaces below (boards/stm32h7b3i_dk/display.*
// and touch.*).

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

// Runs the demo app. Never returns (the threads loop forever); call it from a FreeRTOS
// task and let the task block in it.
void run(FrameSink &sink, TouchSource &touch);

} // namespace app
