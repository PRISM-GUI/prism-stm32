// The demo runtime (see app/runtime.hpp). Threading model, mirroring what model_app does
// on SDL: one thread owns the model and the WidgetTree and is the only one that sets
// fields; the other thread only renders snapshots handed over through a mutex+shared_ptr
// handoff. Input goes through PRISM's public path, app::widget_detail::route_event()
// (include/prism/app/event_routing.hpp), hit-tested against the last *shown* snapshot —
// the same `shown` model_app's dispatch uses.
#include "runtime.hpp"

#include "app/alloc_count.hpp"
#include "app/frame_stats.hpp"

#include <prism/app/event_routing.hpp>
#include <prism/app/widget_tree.hpp>
#include <prism/core/connection.hpp>
#include <prism/core/field.hpp>
#include <prism/input/input_event.hpp>
#include <prism/render/scene_snapshot.hpp>

#include "raster/canvas.hpp"
#include "raster/draw_snapshot.hpp"
#include "raster/fonts.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <thread>

namespace app {
namespace {

constexpr float kWidth = 480.f, kHeight = 272.f;
constexpr auto kPollPeriod = std::chrono::milliseconds(16); // ~60 Hz touch polling
// pthread stacks: the logic thread builds snapshots (tests/prism_qemu used a 64 KiB task
// stack for that), the render thread rasterizes into the framebuffer.
constexpr int kLogicStackBytes = 64 * 1024;
constexpr int kRenderStackBytes = 48 * 1024;

// The handoff between the two threads: `pending` is a freshly built snapshot waiting to be
// drawn, `shown` is the one on screen (input is hit-tested against it).
struct Handoff {
    std::mutex mu;
    std::condition_variable cv;
    std::shared_ptr<const prism::render::SceneSnapshot> pending;
    std::chrono::microseconds pending_update_time{}; // the example's tick() before `pending`
    std::chrono::microseconds pending_build_time{};  // how long `pending` took to build
    std::shared_ptr<const prism::render::SceneSnapshot> shown;
    // The timing summary: the render thread adds each finished frame, the logic thread reads
    // a line once a second. Fixed-size, so neither side allocates per frame.
    FrameStats stats{std::chrono::steady_clock::now(), prism_allocation_count()};
};

using Clock = std::chrono::steady_clock;

std::chrono::microseconds since(Clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
}

using prism::app::WidgetTree;
using prism::core::Point;
using prism::input::InputEvent;
using prism::input::MouseButton;
using prism::input::MouseMove;
using prism::render::SceneSnapshot;

// One touch poll as PRISM pointer events, in window coordinates, against the shown frame.
void route_touch(WidgetTree &tree, Handoff &h, std::optional<Point> now,
                 std::optional<Point> &last)
{
    std::shared_ptr<const SceneSnapshot> snap;
    {
        std::lock_guard lock{h.mu};
        snap = h.shown;
    }
    if (!snap)
        return;
    const auto moved = [](Point a, Point b) {
        return a.x.raw() != b.x.raw() || a.y.raw() != b.y.raw();
    };
    if (now && !last)
        prism::app::widget_detail::route_event(
            tree, snap.get(), MouseButton{*now, prism::input::buttons::left, true});
    else if (!now && last)
        prism::app::widget_detail::route_event(
            tree, snap.get(), MouseButton{*last, prism::input::buttons::left, false});
    else if (now && last && moved(*now, *last))
        prism::app::widget_detail::route_event(
            tree, snap.get(), MouseMove{*now, prism::input::buttons::left_mask});
    last = now;
}

// Builds a snapshot of the tree and hands it to the render thread, with the time the example's
// update took and the build time.
void publish(WidgetTree &tree, Handoff &h, std::uint64_t version,
             std::chrono::microseconds update_time)
{
    const auto start = Clock::now();
    auto snap = tree.build_snapshot(kWidth, kHeight, version);
    tree.clear_dirty();
    const auto build_time = since(start);
    {
        std::lock_guard lock{h.mu};
        h.pending = std::move(snap);
        h.pending_update_time = update_time;
        h.pending_build_time = build_time;
    }
    h.cv.notify_one();
}

// Shows the timing line once a second (the render thread fed the frames into h.stats).
void report_stats(Handoff &h, Example &example)
{
    std::optional<std::string> line;
    {
        std::lock_guard lock{h.mu};
        line = h.stats.tick(Clock::now(), prism_allocation_count());
    }
    if (line)
        example.show_stats(*line);
}

void logic_main(Handoff &h, TouchSource &touch, raster::Fonts &fonts,
                const ExampleFactory &make_example)
{
    std::unique_ptr<Example> example = make_example();
    WidgetTree &tree = example->tree();
    tree.set_text_measurer(&fonts); // layout measures with the same fonts the rasterizer draws with

    // First frame, handed to the render thread like any other: it draws and presents it
    // (and records it as `shown`, which input is hit-tested against).
    std::uint64_t version = 0;
    publish(tree, h, ++version, {});

    std::optional<Point> last_touch;
    for (;;) {
        route_touch(tree, h, touch.read(), last_touch);
        const auto update_start = Clock::now();
        example->tick();
        const auto update_time = since(update_start);
        report_stats(h, *example);
        if (tree.any_dirty())
            publish(tree, h, ++version, update_time);
        std::this_thread::sleep_for(kPollPeriod);
    }
}

void render_main(Handoff &h, FrameSink &sink, const raster::Fonts &fonts)
{
    // One canvas for the whole run, pointed at each frame's back buffer: it keeps its scratch
    // storage, so drawing does not allocate after the first frame.
    raster::Canvas canvas{sink.back(), static_cast<int>(kWidth), static_cast<int>(kHeight),
                          static_cast<int>(kWidth)};
    for (;;) {
        std::shared_ptr<const SceneSnapshot> snap;
        StageTimes times;
        {
            std::unique_lock lock{h.mu};
            h.cv.wait(lock, [&] { return h.pending != nullptr; });
            snap = std::move(h.pending);
            h.pending = nullptr;
            times.update = h.pending_update_time;
            times.snapshot = h.pending_build_time;
        }
        canvas.retarget(sink.back());
        auto start = Clock::now();
        canvas.clear(snap->window.background); // clearing is the caller's job, as with SDL
        raster::draw(canvas, *snap, fonts);
        times.draw = since(start);
        start = Clock::now();
        sink.present();
        times.present = since(start);
        {
            std::lock_guard lock{h.mu};
            h.shown = snap;
            h.stats.add(times);
        }
    }
}

struct RunArgs {
    Handoff *h;
    FrameSink *sink;
    TouchSource *touch;
    raster::Fonts *fonts;
    ExampleFactory *make_example;
};

void *logic_entry(void *p)
{
    auto &a = *static_cast<RunArgs *>(p);
    logic_main(*a.h, *a.touch, *a.fonts, *a.make_example);
    return nullptr;
}

void *render_entry(void *p)
{
    auto &a = *static_cast<RunArgs *>(p);
    render_main(*a.h, *a.sink, *a.fonts);
    return nullptr;
}

} // namespace

void run(FrameSink &sink, TouchSource &touch, ExampleFactory make_example)
{
    // raster::Fonts is the shared text measurer: measure() runs on the logic thread,
    // glyph rasterization on the render thread; the faces are immutable and the glyph
    // cache is mutex-guarded (raster/fonts.hpp).
    raster::Fonts fonts;
    Handoff h;
    RunArgs args{&h, &sink, &touch, &fonts, &make_example};

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_t logic, render;
    int rc = 0;
    pthread_attr_setstacksize(&attr, kLogicStackBytes);
    rc |= pthread_create(&logic, &attr, logic_entry, &args);
    pthread_attr_setstacksize(&attr, kRenderStackBytes);
    rc |= pthread_create(&render, &attr, render_entry, &args);
    pthread_attr_destroy(&attr);
    if (rc != 0)
        return; // no heap for the threads: park (the malloc-failed hook will have fired too)
    pthread_join(logic, nullptr);
    pthread_join(render, nullptr);
}

} // namespace app
