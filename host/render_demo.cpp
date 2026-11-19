// render_demo: lays out a small PRISM model with app::WidgetTree + raster::Fonts as the text
// measurer, draws the snapshot into a 480x272 canvas with raster::draw, and writes the frame as
// demo.ppm (P6) so a human can look at what the board would show.
//
//   host/build/render_demo [out.ppm]     (default demo.ppm)

#include "raster/canvas.hpp"
#include "raster/draw_snapshot.hpp"
#include "raster/fonts.hpp"

#include <prism/app/widget_tree.hpp>
#include <prism/core/field.hpp>
#include <prism/render/scene_snapshot.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace prism::core;
using prism::app::ViewBuilder;
using prism::app::WidgetTree;

namespace {

constexpr int W = 480, H = 272;

struct DemoModel {
    Field<std::string> title{"PRISM raster demo"};
    Field<int> counter{42};
    Field<prism::ui::Label<>> heading{{.value = "counter demo", .heading = true}};
    Field<prism::ui::Button> run{{"Run"}};
    Field<bool> enabled{true};
    Field<prism::ui::Slider<>> volume{{.value = 0.5}};

    void view(ViewBuilder& vb) {
        vb.vstack(title, counter, heading, run, enabled, volume);
    }
};

} // namespace

int main(int argc, char** argv) {
    DemoModel model;
    WidgetTree tree(model);
    raster::Fonts fonts;
    tree.set_text_measurer(&fonts);
    auto snap = tree.build_snapshot(W, H, 1);
    tree.clear_dirty();
    if (!snap) {
        std::fprintf(stderr, "build_snapshot returned null\n");
        return 1;
    }

    std::vector<std::uint32_t> fb(static_cast<std::size_t>(W) * H);
    raster::Canvas canvas{fb, W, H, W};
    canvas.clear(snap->window.background);   // clear is the caller's job
    raster::draw(canvas, *snap, fonts);

    const char* path = argc > 1 ? argv[1] : "demo.ppm";
    std::FILE* f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (std::uint32_t p : fb) {
        const std::uint8_t rgb[3] = {static_cast<std::uint8_t>((p >> 16) & 0xFF),
                                     static_cast<std::uint8_t>((p >> 8) & 0xFF),
                                     static_cast<std::uint8_t>(p & 0xFF)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::printf("wrote %s: %zu draw commands, %zu widgets\n", path, snap->draw_command_count,
                snap->geometry.size());
    return 0;
}
