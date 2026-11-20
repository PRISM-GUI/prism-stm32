// Drawing must not allocate once warmed up: a canvas reused across frames (retarget()) draws
// the same content a second time with no heap allocation. Counted with the host's operator new
// counter (host/alloc_count_host.cpp, linked into this test).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/alloc_count.hpp"
#include "app/audio_scope.hpp"
#include "app/synthetic_source.hpp"
#include "raster/canvas.hpp"
#include "raster/draw_snapshot.hpp"
#include "raster/fonts.hpp"

#include <vector>

using namespace prism::core;

namespace {
constexpr int W = 480, H = 272;

} // namespace

TEST_CASE("raster: a reused canvas draws a thick polyline without allocating")
{
    std::vector<std::uint32_t> a(std::size_t(W) * H), b(std::size_t(W) * H);
    std::vector<Point> zigzag;
    for (int i = 0; i < 100; ++i)
        zigzag.push_back(Point{X{float(4 * i)}, Y{float(i % 2 ? 40 : 120)}});
    const Rect clip{Point{X{10.f}, Y{10.f}}, Size{Width{400.f}, Height{200.f}}};

    raster::Canvas canvas{a, W, H, W};
    canvas.push_clip(clip);
    canvas.polyline(zigzag, Color::rgba(0, 140, 200), 2.f); // warm-up frame
    canvas.pop_clip();

    canvas.retarget(b);
    const auto before = prism_allocation_count();
    canvas.push_clip(clip);
    canvas.polyline(zigzag, Color::rgba(0, 140, 200), 2.f);
    canvas.pop_clip();
    CHECK(prism_allocation_count() - before == 0);
    CHECK(a == b); // the same pixels, in the new target
}

TEST_CASE("raster: the second frame of the scope draws without allocating")
{
    app::SyntheticSource source{16000.f};
    app::AudioScope scope{source};
    raster::Fonts fonts;
    scope.tree().set_text_measurer(&fonts);
    scope.tick();
    auto snap = scope.tree().build_snapshot(W, H, 1);
    std::vector<std::uint32_t> a(std::size_t(W) * H), b(std::size_t(W) * H);

    raster::Canvas canvas{a, W, H, W};
    raster::draw(canvas, *snap, fonts); // warm-up: glyph cache, scratch buffers

    canvas.retarget(b);
    const auto before = prism_allocation_count();
    raster::draw(canvas, *snap, fonts);
    CHECK(prism_allocation_count() - before == 0);
}
