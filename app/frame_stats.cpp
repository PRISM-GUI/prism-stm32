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
    std::snprintf(buf, sizeof buf, "%.1f ms", double(sum.count()) / 1000.0 / frames);
    return buf;
}

} // namespace

void FrameStats::add(StageTimes t)
{
    sum_.snapshot += t.snapshot;
    sum_.draw += t.draw;
    sum_.present += t.present;
    ++frames_;
}

std::optional<std::string> FrameStats::tick(std::chrono::steady_clock::time_point now)
{
    const std::chrono::duration<double> elapsed = now - window_start_;
    if (elapsed.count() < 1.0)
        return std::nullopt;
    const long fps = std::lround(frames_ / elapsed.count());
    std::string line = std::to_string(fps) + " fps · snapshot " +
                       average_ms(sum_.snapshot, frames_) + " · draw " +
                       average_ms(sum_.draw, frames_) + " · present " +
                       average_ms(sum_.present, frames_);
    window_start_ = now;
    sum_ = {};
    frames_ = 0;
    return line;
}

} // namespace app
