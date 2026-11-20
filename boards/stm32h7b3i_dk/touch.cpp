// Touch polling for the STM32H7B3I-DK (see touch.hpp).
#include "touch.hpp"

#include "display.hpp"   // Display::kWidth/kHeight: the panel is the LCD's 480x272
#include "stm32h7b3i_discovery_ts.h"

#include <chrono>
#include <thread>

bool Touch::init()
{
    TS_Init_t ts_init = {};
    ts_init.Width       = Display::kWidth;
    ts_init.Height      = Display::kHeight;
    ts_init.Orientation = TS_SWAP_NONE;
    ts_init.Accuracy    = 2;
    // The touch controller powers up with the LCD and does not answer I2C for a few hundred
    // ms after it: retry for up to 2 s instead of giving up on the first probe.
    for (int attempt = 0; attempt < 20 && !ready_; ++attempt) {
        ready_ = BSP_TS_Init(0, &ts_init) == BSP_ERROR_NONE;
        if (!ready_)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return ready_;
}

std::optional<prism::core::Point> Touch::read()
{
    if (!ready_)
        return std::nullopt;
    TS_State_t state = {};
    if (BSP_TS_GetState(0, &state) != BSP_ERROR_NONE || state.TouchDetected == 0U)
        return std::nullopt;
    // BSP_TS_GetState already scaled to the 480x272 panel and applied the orientation.
    return prism::core::Point{prism::core::X{static_cast<float>(state.TouchX)},
                              prism::core::Y{static_cast<float>(state.TouchY)}};
}
