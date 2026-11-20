#pragma once

// Per-frame timing, summarized once a second into the line the examples show on screen:
// "24 fps · snapshot 9.1 ms · draw 22.4 ms · present 0.3 ms".

#include <chrono>
#include <optional>
#include <string>

namespace app {

// One finished frame: snapshot build (logic thread), draw and present (render thread).
struct StageTimes {
    std::chrono::microseconds snapshot{}, draw{}, present{};
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
