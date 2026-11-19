#pragma once

// A trivial fixed-width text measurer for the QEMU firmware: every glyph is
// 8 px wide and every line is 16 px tall, so layout is deterministic (the
// board has no font engine). Implements ui::TextMeasurer, which the widget
// tree reads through Theme::measurer (set with WidgetTree::set_text_measurer,
// read only on the logic thread).
#include <prism/core/utf8.hpp>
#include <prism/ui/text_metrics.hpp>

#include <string_view>

namespace prism_qemu {

struct FixedMeasurer final : prism::ui::TextMeasurer {
    static constexpr float glyph_w = 8.f;
    static constexpr float line_h = 16.f;

    float width(std::string_view utf8, float /*size*/,
                prism::render::FontStyle /*font*/) const override {
        return glyph_w * static_cast<float>(prism::core::utf8::count(utf8));
    }

    float line_height(float /*size*/, prism::render::FontStyle /*font*/) const override {
        return line_h;
    }
};

} // namespace prism_qemu
