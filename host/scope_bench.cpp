// The audio scope on the host, as fast as it goes: each frame pulls the synthetic sweep, builds
// a snapshot and draws it into a 480x272 canvas, timed by the same FrameStats the board shows.
// Prints a stats line per second, then writes the last frame to scope.ppm (or argv[1]).
//
//   host/build/scope_bench [out.ppm] [frames]

#include <embedded/audio_scope.hpp>
#include <embedded/synthetic_source.hpp>

#include <prism/app/widget_tree.hpp>
#include <prism/platform/alloc_count.hpp>
#include <prism/platform/frame_stats.hpp>
#include <prism/raster/canvas.hpp>
#include <prism/raster/draw_snapshot.hpp>
#include <prism/raster/fonts.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int W = 480, H = 272;
using Clock = std::chrono::steady_clock;

std::chrono::microseconds since(Clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
}

bool write_ppm(const char *path, const std::vector<std::uint32_t> &fb)
{
    std::FILE *f = std::fopen(path, "wb");
    if (!f)
        return false;
    std::fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (std::uint32_t p : fb) {
        const std::uint8_t rgb[3] = {std::uint8_t(p >> 16), std::uint8_t(p >> 8), std::uint8_t(p)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "scope.ppm";
    const int frames = argc > 2 ? std::atoi(argv[2]) : 300;

    prism_examples::SyntheticSource source{16000.f};
    prism_examples::ScopeState state{source};
    prism_examples::ScopeModel model;
    prism_examples::scope_bind(model, state);
    prism::app::WidgetTree tree{model};
    prism::raster::Fonts fonts;
    tree.set_text_measurer(&fonts);
    std::vector<std::uint32_t> fb(std::size_t(W) * H);
    prism::raster::Canvas canvas{fb, W, H, W};
    using prism::platform::allocation_count;
    prism::platform::FrameStats stats{Clock::now(), allocation_count()};

    prism::render::SceneSnapshot snap; // rebuilt in place every frame, as the board's slots are
    std::uint32_t allocs[3] = {}; // per stage, in the last frame: update, snapshot, draw
    for (std::uint64_t version = 1; version <= std::uint64_t(frames); ++version) {
        prism::platform::StageTimes t;
        auto count = allocation_count();
        auto start = Clock::now();
        prism_examples::scope_tick(model, state);
        t.update = since(start);
        allocs[0] = allocation_count() - count;
        count = allocation_count();
        start = Clock::now();
        tree.build_snapshot(snap, W, H, version);
        tree.clear_dirty();
        t.snapshot = since(start);
        allocs[1] = allocation_count() - count;
        count = allocation_count();
        start = Clock::now();
        canvas.clear(snap.window.background);
        prism::raster::draw(canvas, snap, fonts);
        t.draw = since(start);
        allocs[2] = allocation_count() - count;
        stats.add(t);
        if (auto line = stats.tick(Clock::now(), allocation_count())) {
            std::printf("%.*s\n", int(line->size()), line->data());
            prism_examples::scope_show_stats(model, state, *line);
        }
    }
    std::printf("allocations in the last frame: update %u, snapshot %u, draw %u\n",
                unsigned(allocs[0]), unsigned(allocs[1]), unsigned(allocs[2]));
    if (!write_ppm(path, fb)) {
        std::fprintf(stderr, "cannot write %s\n", path);
        return 1;
    }
    std::printf("wrote %s\n", path);
    return 0;
}
