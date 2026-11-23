#pragma once

// The board's touch panel: the FT5336 on I2C3 (SCL PH7, SDA PH8), read by polling the chip's
// registers (no interrupt). Implements PRISM's TouchSource (prism/platform/frame_target.hpp):
// read() returns the first touch's position in panel pixels (0..479, 0..271 — the same
// coordinates the 480x272 framebuffer uses), or nullopt when no finger is down.

#include <prism/platform/frame_target.hpp>

#include <optional>

class Touch final : public prism::platform::TouchSource {
public:
    // I2C3 up, then FT5336_Init/ReadID with retries (the controller answers a few hundred ms
    // after power-up). False when it never answers: read() then reports no touch, so the screen
    // still runs.
    bool init();

    std::optional<prism::core::Point> read() override;

private:
    bool ready_ = false;   // init failed: read() reports no touch, so the screen still runs
};
