#pragma once

// The board's touch panel: the FT5336 on I2C4, read by polling BSP_TS_GetState (no
// interrupt). Implements app::TouchSource (app/runtime.hpp): read() returns the first
// touch's position in panel pixels (0..479, 0..271 — the same coordinates the 480x272
// framebuffer uses), or nullopt when no finger is down.

#include "app/runtime.hpp"

#include <optional>

class Touch final : public app::TouchSource {
public:
    // BSP_TS_Init probes the GT911 first (I2C4 0xBA), then the FT5336 (0x70 = 7-bit 0x38);
    // width/height 480x272, axes swapped for the FT5336 as in ST's example, accuracy 2 px.
    bool init();

    std::optional<prism::core::Point> read() override;

private:
    bool ready_ = false;   // init failed: read() reports no touch, so the screen still runs
};
