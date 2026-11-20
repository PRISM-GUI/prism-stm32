// The audio scope on the host, as fast as it goes: each frame pulls the synthetic sweep, builds
// a snapshot and draws it into a 480x272 canvas, timed by the same FrameStats the board shows.
// Prints a stats line per second, then writes the last frame to scope.ppm (or argv[1]).
//
//   host/build/scope_bench [out.ppm] [frames]

#include "app/alloc_count.hpp"
#include "app/audio_scope.hpp"
#include "app/frame_stats.hpp"
#include "app/synthetic_source.hpp"
#include "raster/canvas.hpp"
#include "raster/draw_snapshot.hpp"
#include "raster/fonts.hpp"

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

    app::SyntheticSource source{16000.f};
    app::AudioScope scope{source};
    raster::Fonts fonts;
    scope.tree().set_text_measurer(&fonts);
    std::vector<std::uint32_t> fb(std::size_t(W) * H);
    raster::Canvas canvas{fb, W, H, W};
    app::FrameStats stats{Clock::now(), prism_allocation_count()};

    prism::render::SceneSnapshot snap; // rebuilt in place every frame, as the board's slots are
    std::uint32_t allocs[3] = {}; // per stage, in the last frame: update, snapshot, draw
    for (std::uint64_t version = 1; version <= std::uint64_t(frames); ++version) {
        app::StageTimes t;
        auto count = prism_allocation_count();
        auto start = Clock::now();
        scope.tick();
        t.update = since(start);
        allocs[0] = prism_allocation_count() - count;
        count = prism_allocation_count();
        start = Clock::now();
        scope.tree().build_snapshot(snap, W, H, version);
        scope.tree().clear_dirty();
        t.snapshot = since(start);
        allocs[1] = prism_allocation_count() - count;
        count = prism_allocation_count();
        start = Clock::now();
        canvas.clear(snap.window.background);
        raster::draw(canvas, snap, fonts);
        t.draw = since(start);
        allocs[2] = prism_allocation_count() - count;
        stats.add(t);
        if (auto line = stats.tick(Clock::now(), prism_allocation_count())) {
            std::printf("%.*s\n", int(line->size()), line->data());
            scope.show_stats(*line);
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
