#pragma once

// Per-frame timing, summarized once a second into the line the examples show on screen:
// "24 fps · 412 allocs/frame · update 1.20 · snapshot 9.10 · draw 22.40 · present 0.30 ms"
// (averages; allocations are heap allocations by any thread, per frame drawn).

#include <chrono>
#include <cstdint>
#include <array>
#include <optional>
#include <string_view>

namespace app {

// One finished frame: the example's update (e.g. FFT and plot data) and the snapshot build
// on the logic thread, then draw and present on the render thread.
struct StageTimes {
    std::chrono::microseconds update{}, snapshot{}, draw{}, present{};
};

class FrameStats {
public:
    // `allocations` is a running count of heap allocations (it may wrap around), read now.
    explicit FrameStats(std::chrono::steady_clock::time_point start, std::uint32_t allocations = 0)
        : window_start_(start), window_allocations_(allocations) {}

    void add(StageTimes t);
    // The summary of the last second once a second has passed since the previous one, then
    // starts a new second; nullopt before that. `allocations`: the running count, read now.
    // The line lives in this object's buffer until the next tick: no allocation.
    std::optional<std::string_view> tick(std::chrono::steady_clock::time_point now,
                                         std::uint32_t allocations);

private:
    std::chrono::steady_clock::time_point window_start_;
    std::uint32_t window_allocations_;
    StageTimes sum_{};
    int frames_ = 0;
    std::array<char, 160> line_{};
};

} // namespace app
