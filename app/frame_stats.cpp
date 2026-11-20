#include "app/frame_stats.hpp"

#include <cmath>
#include <cstdio>

namespace app {
namespace {

std::string average_ms(std::chrono::microseconds sum, int frames)
{
    if (frames == 0)
        return "-";
    char buf[24];
    std::snprintf(buf, sizeof buf, "%.2f", double(sum.count()) / 1000.0 / frames);
    return buf;
}

std::string per_frame(std::uint32_t count, int frames)
{
    return frames == 0 ? "-" : std::to_string((count + std::uint32_t(frames) / 2) / std::uint32_t(frames));
}

} // namespace

void FrameStats::add(StageTimes t)
{
    sum_.update += t.update;
    sum_.snapshot += t.snapshot;
    sum_.draw += t.draw;
    sum_.present += t.present;
    ++frames_;
}

std::optional<std::string> FrameStats::tick(std::chrono::steady_clock::time_point now,
                                            std::uint32_t allocations)
{
    const std::chrono::duration<double> elapsed = now - window_start_;
    if (elapsed.count() < 1.0)
        return std::nullopt;
    const long fps = std::lround(frames_ / elapsed.count());
    std::string line = std::to_string(fps) + " fps \u00b7 " +
                       per_frame(allocations - window_allocations_, frames_) +
                       " allocs/frame \u00b7 update " +
                       average_ms(sum_.update, frames_) + " \u00b7 snapshot " +
                       average_ms(sum_.snapshot, frames_) + " \u00b7 draw " +
                       average_ms(sum_.draw, frames_) + " \u00b7 present " +
                       average_ms(sum_.present, frames_) + " ms";
    window_start_ = now;
    window_allocations_ = allocations;
    sum_ = {};
    frames_ = 0;
    return line;
}

} // namespace app
