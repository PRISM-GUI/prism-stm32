// The PRISM demo model, board-independent (app/). A hand-written view() — this toolchain
// has no reflection — over a title label, a counter, a "+1" button, a checkbox and a
// slider. Widgets are the delegates' default size (30 px tall, slider 30 px with a
// caption) plus explicit gaps, which on the 480x272 panel is as finger-sized as the
// layout gets: ViewBuilder has no per-leaf size hints (only CanvasHandle.min_size /
// TextHandle.rows), so the layout's defaults + gaps are what we rely on.
#pragma once

#include <prism/app/widget_tree.hpp>
#include <prism/core/field.hpp>

namespace app {

struct DemoModel {
    prism::core::Field<prism::ui::Label<>> title{{.value = "PRISM on STM32H7", .heading = true}};
    prism::core::Field<int> counter{0};
    prism::core::Field<prism::ui::Button> increment{{.text = "+1"}};
    prism::core::Field<prism::ui::Checkbox> enabled{{.checked = true, .label = "enabled"}};
    prism::core::Field<prism::ui::Slider<>> volume{{.value = 0.5, .min = 0.0, .max = 1.0, .step = 0.05, .label = "volume"}};

    void view(prism::app::WidgetTree::ViewBuilder &vb) {
        vb.vstack([&] {
            vb.widget(title);
            vb.gap(12.f);
            vb.widget(counter);
            vb.gap(12.f);
            vb.hstack([&] {
                vb.widget(increment);
                vb.gap(16.f);
                vb.widget(enabled);
            });
            vb.gap(12.f);
            vb.widget(volume);
        });
    }
};

} // namespace app
