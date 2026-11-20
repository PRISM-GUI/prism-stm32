#pragma once

// What the runtime (app/runtime.hpp) runs: a model, its WidgetTree, and the work the logic
// thread does each loop besides input. The widget demo and the audio scope are Examples.

#include <prism/app/widget_tree.hpp>

#include <functional>
#include <memory>
#include <string_view>

namespace app {

struct Example {
    virtual prism::app::WidgetTree &tree() = 0;
    // Each logic-thread loop, after input and before the dirty check (e.g. pull new samples).
    virtual void tick() = 0;
    // The FrameStats line (app/frame_stats.hpp), once a second, on the logic thread.
    virtual void show_stats(std::string_view line) = 0;
    virtual ~Example() = default;
};

// Called on the logic thread, which then owns the Example (fields are set there only).
// A std::function so board code can capture its sample source.
using ExampleFactory = std::function<std::unique_ptr<Example>()>;

} // namespace app
