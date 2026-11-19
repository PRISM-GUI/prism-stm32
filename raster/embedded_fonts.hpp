#pragma once

// The font files compiled into the firmware, as byte spans. The arrays are generated at build
// time by scripts/embed.py (a meson custom_target): GCC 15 supports #embed in C as an extension
// but not in this C++ TU, so a small Python script emits the arrays instead.
//
// assets/NotoSans-Latin.ttf and assets/NotoSans-Bold-Latin.ttf, SIL OFL (assets/NotoSans-OFL.txt).

#include <cstddef>
#include <cstdint>
#include <span>

namespace raster::embedded {

std::span<const std::uint8_t> noto_sans_latin();
std::span<const std::uint8_t> noto_sans_bold_latin();

} // namespace raster::embedded
