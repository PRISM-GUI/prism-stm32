#pragma once

// The widget demo (app/demo.hpp) as an Example: "+1" increments the counter.

#include "app/demo.hpp"
#include "app/example.hpp"

#include <prism/core/connection.hpp>

namespace app {

class WidgetsExample final : public Example {
public:
    WidgetsExample()
        : on_click_(model_.increment.on_change().connect([this](const prism::ui::Button &) {
              model_.counter.set(model_.counter.get() + 1);
          }))
    {
    }

    prism::app::WidgetTree &tree() override { return tree_; }
    void tick() override {}
    // simplify: the widget demo has no place for the timing line; the scope shows it.
    void show_stats(std::string_view) override {}

private:
    DemoModel model_;
    prism::app::WidgetTree tree_{model_};
    prism::core::Connection on_click_;
};

inline std::unique_ptr<Example> make_widgets_example()
{
    return std::make_unique<WidgetsExample>();
}

} // namespace app
