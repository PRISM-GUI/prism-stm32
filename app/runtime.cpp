// The demo runtime (see app/runtime.hpp). Threading model, mirroring what model_app does
// on SDL: one thread owns the model and the WidgetTree and is the only one that sets
// fields; the other thread only renders snapshots handed over through a mutex+shared_ptr
// handoff. Input goes through PRISM's public path, app::widget_detail::route_event()
// (include/prism/app/event_routing.hpp), hit-tested against the last *shown* snapshot —
// the same `shown` model_app's dispatch uses.
#include "runtime.hpp"

#include "demo.hpp"

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
    std::shared_ptr<const prism::render::SceneSnapshot> shown;
};

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

void logic_main(Handoff &h, TouchSource &touch, raster::Fonts &fonts)
{
    DemoModel model;
    WidgetTree tree(model);
    tree.set_text_measurer(&fonts); // layout measures with the same fonts the rasterizer draws with

    // The "+1" button increments the counter; the click fires during dispatch, on this thread.
    prism::core::Connection on_click = model.increment.on_change().connect(
        [&model](const prism::ui::Button &) { model.counter.set(model.counter.get() + 1); });

    // First frame, handed to the render thread like any other: it draws and presents it
    // (and records it as `shown`, which input is hit-tested against).
    std::uint64_t version = 0;
    {
        auto snap = tree.build_snapshot(kWidth, kHeight, ++version);
        tree.clear_dirty();
        std::lock_guard lock{h.mu};
        h.pending = std::move(snap);
    }
    h.cv.notify_one();

    std::optional<Point> last_touch;
    for (;;) {
        route_touch(tree, h, touch.read(), last_touch);
        if (tree.any_dirty()) {
            auto snap = tree.build_snapshot(kWidth, kHeight, ++version);
            tree.clear_dirty();
            {
                std::lock_guard lock{h.mu};
                h.pending = std::move(snap);
            }
            h.cv.notify_one();
        }
        std::this_thread::sleep_for(kPollPeriod);
    }
}

void render_main(Handoff &h, FrameSink &sink, const raster::Fonts &fonts)
{
    for (;;) {
        std::shared_ptr<const SceneSnapshot> snap;
        {
            std::unique_lock lock{h.mu};
            h.cv.wait(lock, [&] { return h.pending != nullptr; });
            snap = std::move(h.pending);
            h.pending = nullptr;
        }
        raster::Canvas canvas{sink.back(), static_cast<int>(kWidth), static_cast<int>(kHeight),
                              static_cast<int>(kWidth)};
        canvas.clear(snap->window.background); // clearing is the caller's job, as with SDL
        raster::draw(canvas, *snap, fonts);
        sink.present();
        {
            std::lock_guard lock{h.mu};
            h.shown = snap;
        }
    }
}

struct RunArgs {
    Handoff *h;
    FrameSink *sink;
    TouchSource *touch;
    raster::Fonts *fonts;
};

void *logic_entry(void *p)
{
    auto &a = *static_cast<RunArgs *>(p);
    logic_main(*a.h, *a.touch, *a.fonts);
    return nullptr;
}

void *render_entry(void *p)
{
    auto &a = *static_cast<RunArgs *>(p);
    render_main(*a.h, *a.sink, *a.fonts);
    return nullptr;
}

} // namespace

void run(FrameSink &sink, TouchSource &touch)
{
    // raster::Fonts is the shared text measurer: measure() runs on the logic thread,
    // glyph rasterization on the render thread; the faces are immutable and the glyph
    // cache is mutex-guarded (raster/fonts.hpp).
    raster::Fonts fonts;
    Handoff h;
    RunArgs args{&h, &sink, &touch, &fonts};

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
