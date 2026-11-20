// attach_widgets and attach_scope under PRISM's frame runtime: a memory FrameSink, no touch, and
// a quit on the first timing line. The hooks the app registered ran before that line was handed
// on, so what they wrote is in the models.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/audio_scope.hpp"
#include "app/widgets_app.hpp"

#include <prism/platform/frame_app.hpp>

#include <algorithm>
#include <cmath>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {
constexpr int W = 480, H = 272;

struct MemorySink final : prism::platform::FrameSink {
    std::vector<std::uint32_t> pixels = std::vector<std::uint32_t>(std::size_t(W) * H);
    std::span<std::uint32_t> back() override { return pixels; }
    void present() override {}
    prism::core::Size size() const override
    {
        return {prism::core::Width{float(W)}, prism::core::Height{float(H)}};
    }
};

struct NoTouch final : prism::platform::TouchSource {
    std::optional<prism::core::Point> read() override { return std::nullopt; }
};

struct QueuedSource final : app::SampleSource {
    std::deque<float> queued;
    std::size_t read(std::span<float> out) override
    {
        const auto n = std::min(out.size(), queued.size());
        std::copy_n(queued.begin(), n, out.begin());
        queued.erase(queued.begin(), queued.begin() + std::ptrdiff_t(n));
        return n;
    }
    float sample_rate() const override { return 16000.f; }
    const char *name() const override { return "queued"; }
};

void quit_on_first_line(prism::platform::FrameContext &ctx)
{
    ctx.on_stats([&ctx](std::string_view) { ctx.quit(); });
}
} // namespace

TEST_CASE("attach_widgets puts the timing line in the stats label")
{
    app::WidgetsModel model;
    MemorySink sink;
    NoTouch touch;
    prism::platform::frame_app(sink, touch, model, [&](prism::platform::FrameContext &ctx) {
        app::attach_widgets(ctx, model);
        quit_on_first_line(ctx);
    });
    const std::string stats = model.stats.get().value;
    CHECK(stats.find("fps") != std::string::npos);
}

TEST_CASE("attach_scope drains the source and reports its level in the title")
{
    QueuedSource source;
    for (int i = 0; i < 2000; ++i)
        source.queued.push_back(0.5f * std::sin(0.1f * float(i)));
    app::ScopeState state{source};
    app::ScopeModel model;
    MemorySink sink;
    NoTouch touch;
    prism::platform::frame_app(sink, touch, model, [&](prism::platform::FrameContext &ctx) {
        app::attach_scope(ctx, model, state);
        quit_on_first_line(ctx);
    });
    CHECK(source.queued.empty());
    const std::string title = model.title.get().value;
    CHECK(title.find("dBFS") != std::string::npos);
    CHECK(model.stats.get().value.find("fps") != std::string::npos);
}
