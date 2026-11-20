#include "app/frame_stats.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace app {
namespace {

// A short field of the line, in a small fixed buffer: "-" when no frame was drawn.
struct Field16 {
    char text[16];
};

Field16 average_ms(std::chrono::microseconds sum, int frames)
{
    Field16 f{"-"};
    if (frames != 0)
        std::snprintf(f.text, sizeof f.text, "%.2f", double(sum.count()) / 1000.0 / frames);
    return f;
}

Field16 per_frame(std::uint32_t count, int frames)
{
    Field16 f{"-"};
    if (frames != 0)
        std::snprintf(f.text, sizeof f.text, "%lu",
                      static_cast<unsigned long>((count + std::uint32_t(frames) / 2) / std::uint32_t(frames)));
    return f;
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

std::optional<std::string_view> FrameStats::tick(std::chrono::steady_clock::time_point now,
                                                 std::uint32_t allocations)
{
    const std::chrono::duration<double> elapsed = now - window_start_;
    if (elapsed.count() < 1.0)
        return std::nullopt;
    const long fps = std::lround(frames_ / elapsed.count());
    const int n = std::snprintf(
        line_.data(), line_.size(),
        "%ld fps · %s allocs/frame · update %s · snapshot %s · draw %s · present %s ms",
        fps, per_frame(allocations - window_allocations_, frames_).text,
        average_ms(sum_.update, frames_).text, average_ms(sum_.snapshot, frames_).text,
        average_ms(sum_.draw, frames_).text, average_ms(sum_.present, frames_).text);
    window_start_ = now;
    window_allocations_ = allocations;
    sum_ = {};
    frames_ = 0;
    return std::string_view(line_.data(), std::size_t(std::min<int>(n, int(line_.size()) - 1)));
}

} // namespace app
