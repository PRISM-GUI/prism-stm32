#pragma once

// Per-frame timing, summarized once a second into the line the examples show on screen:
// "24 fps · update 1.20 · snapshot 9.10 · draw 22.40 · present 0.30 ms" (averages, in ms).

#include <chrono>
#include <optional>
#include <string>

namespace app {

// One finished frame: the example's update (e.g. FFT and plot data) and the snapshot build
// on the logic thread, then draw and present on the render thread.
struct StageTimes {
    std::chrono::microseconds update{}, snapshot{}, draw{}, present{};
};

class FrameStats {
public:
    explicit FrameStats(std::chrono::steady_clock::time_point start) : window_start_(start) {}

    void add(StageTimes t);
    // The summary of the last second once a second has passed since the previous one, then
    // starts a new second; nullopt before that.
    std::optional<std::string> tick(std::chrono::steady_clock::time_point now);

private:
    std::chrono::steady_clock::time_point window_start_;
    StageTimes sum_{};
    int frames_ = 0;
};

} // namespace app
