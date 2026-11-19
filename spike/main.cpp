// Spike: a PRISM model whose widget tree is laid out and turned into a snapshot, on the
// Cortex-M7. No display yet: the point is that it compiles, links, and how big it is.
#include <prism/core/field.hpp>
#include <prism/app/widget_tree.hpp>

struct Counter {
    prism::core::Field<int> count{0};
    prism::core::Field<std::string> label{"Hello, STM32"};
    void view(prism::app::ViewBuilder& vb) { vb.widget(label); vb.widget(count); }
};

int main() {
    Counter model;
    prism::app::WidgetTree tree(model);
    model.count.set(1);
    auto snap = tree.build_snapshot(480, 272, 1);
    return snap ? 0 : 1;
}
