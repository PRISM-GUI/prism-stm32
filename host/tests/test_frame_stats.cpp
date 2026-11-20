#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/frame_stats.hpp"

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {
app::StageTimes frame(int update_ms, int snapshot_ms, int draw_ms, int present_ms)
{
    return {std::chrono::milliseconds(update_ms), std::chrono::milliseconds(snapshot_ms),
            std::chrono::milliseconds(draw_ms), std::chrono::milliseconds(present_ms)};
}
} // namespace

TEST_CASE("frame stats: no line before a second has passed")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add(frame(5, 2, 10, 1));
    CHECK_FALSE(stats.tick(start + 500ms).has_value());
}

TEST_CASE("frame stats: one line per second with fps and average stage times")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add(frame(5, 2, 10, 1));
    stats.add(frame(7, 4, 20, 1));
    CHECK(stats.tick(start + 1s) == "2 fps \u00b7 update 6.00 \u00b7 snapshot 3.00 \u00b7 draw 15.00 \u00b7 present 1.00 ms");
}

TEST_CASE("frame stats: a second without frames reads 0 fps, not the previous numbers")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add(frame(5, 2, 10, 1));
    (void)stats.tick(start + 1s);
    CHECK(stats.tick(start + 2s) == "0 fps \u00b7 update - \u00b7 snapshot - \u00b7 draw - \u00b7 present - ms");
}

TEST_CASE("frame stats: sub-millisecond stages keep their precision")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add({std::chrono::microseconds(120), std::chrono::microseconds(450),
               std::chrono::microseconds(8300), std::chrono::microseconds(30)});
    CHECK(stats.tick(start + 1s) == "1 fps \u00b7 update 0.12 \u00b7 snapshot 0.45 \u00b7 draw 8.30 \u00b7 present 0.03 ms");
}
