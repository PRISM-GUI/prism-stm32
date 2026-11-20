#pragma once

// The firmware's two Examples, for the old runtime (app/runtime.hpp): the widget demo
// (app/demo.hpp, "+1" increments the counter) and the audio scope over its model and state
// (app/audio_scope.hpp). simplify: a bridge while the firmware still runs Examples; Task 8 moves
// it to frame_app (attach_scope, app/widgets_app.hpp) and deletes this file.

#include "app/audio_scope.hpp"
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

class ScopeExample final : public Example {
public:
    explicit ScopeExample(SampleSource &source) : state_(source) { scope_bind(model_, state_); }

    prism::app::WidgetTree &tree() override { return tree_; }
    void tick() override { scope_tick(model_, state_); }
    void show_stats(std::string_view line) override { scope_show_stats(model_, state_, line); }

private:
    ScopeState state_; // first: the model's plots read its buffers
    ScopeModel model_;
    prism::app::WidgetTree tree_{model_};
};

inline std::unique_ptr<Example> make_scope_example(SampleSource &source)
{
    return std::make_unique<ScopeExample>(source);
}

} // namespace app
