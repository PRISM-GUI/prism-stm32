#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "app/frame_stats.hpp"

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

namespace {
app::StageTimes frame(int snapshot_ms, int draw_ms, int present_ms)
{
    return {std::chrono::milliseconds(snapshot_ms), std::chrono::milliseconds(draw_ms),
            std::chrono::milliseconds(present_ms)};
}
} // namespace

TEST_CASE("frame stats: no line before a second has passed")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add(frame(2, 10, 1));
    CHECK_FALSE(stats.tick(start + 500ms).has_value());
}

TEST_CASE("frame stats: one line per second with fps and average stage times")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add(frame(2, 10, 1));
    stats.add(frame(4, 20, 1));
    CHECK(stats.tick(start + 1s) == "2 fps · snapshot 3.0 ms · draw 15.0 ms · present 1.0 ms");
}

TEST_CASE("frame stats: a second without frames reads 0 fps, not the previous numbers")
{
    const auto start = Clock::now();
    app::FrameStats stats{start};
    stats.add(frame(2, 10, 1));
    (void)stats.tick(start + 1s);
    CHECK(stats.tick(start + 2s) == "0 fps · snapshot - · draw - · present -");
}
