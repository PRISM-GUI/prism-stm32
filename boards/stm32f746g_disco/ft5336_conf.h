#pragma once

// Configuration for the FT5336 capacitive controller on the STM32F746G-DISCO's RK043FN48H panel
// (see third_party/stm32-ft5336/ft5336_conf_template.h). Included by ft5336.h.

// The internal calibration needs a finger-free screen at power-up and takes ~200 ms; ST's F7 BSP
// leaves it out too (the panel reports usable coordinates either way).
#define FT5336_AUTO_CALIBRATION_ENABLED 0U

// The panel's own resolution, in the axes the controller reports (which are swapped, see touch.cpp).
#define FT5336_MAX_X_LENGTH 480U
#define FT5336_MAX_Y_LENGTH 272U
