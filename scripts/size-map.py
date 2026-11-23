#!/usr/bin/env python3
"""Per-origin flash breakdown of a linked prism-stm32 firmware.

Usage, from the build directory (the map must come from a stripped, cref-enabled link so that
non-allocated debug sections do not inflate anything):

  ninja -t commands stm32f746g_disco.elf | tail -1 > /tmp/l.sh
  sed -i 's|$| -Wl,-S -Wl,--cref -Wl,-Map=/tmp/map.txt|' /tmp/l.sh
  sh /tmp/l.sh
  python3 scripts/size-map.py /tmp/map.txt

The map's input-section lines carry the size and the contributing object, so summing them per
object and then per library family gives the split that `arm-none-eabi-size` cannot show.
"""

import re
import sys
import collections

path = sys.argv[1]
lines = open(path, errors="ignore").read().splitlines()
start = max(
    i for i, l in enumerate(lines) if l.startswith("Linker script and memory map")
)

# Input-section lines: an address, a size and the contributing object file.
line_re = re.compile(r"^\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(\S.*)$")


def family(p):
    if "cmsis-dsp" in p:
        return "cmsis-dsp (FFT tables/code)"
    if "libunibreak" in p:
        return "libunibreak"
    if "sheenbidi" in p.lower():
        return "SheenBidi"
    if "libfmt" in p:
        return "fmt"
    if "libstdc++" in p:
        return "libstdc++"
    if re.search(r"/libc\.a|/libm\.a|/libg\.a|libnosys|libgcc", p):
        return "newlib + libgcc"
    if "libstm32_hal" in p:
        return "HAL"
    if "libboard_bsp" in p:
        return "BSP components"
    if "libfreertos_board" in p:
        return "FreeRTOS"
    if "libposix_board" in p:
        return "PRISM posix/newlib layer"
    if "libprism-raster" in p:
        return "prism raster (+fonts)"
    if "libprism-platform" in p:
        return "prism platform"
    if "libprism_embedded_apps" in p:
        return "prism embedded apps (scope/widgets)"
    if "libprism.a" in p:
        return "prism core/app"
    if "libboard_objects" in p or "stm32f746g_disco.elf.p" in p:
        return "board (ours)"
    return "other"


sizes = collections.Counter()
fams = collections.Counter()
for line in lines[start:]:
    m = line_re.match(line)
    if not m:
        continue
    size, target = m.group(2), m.group(3).strip()
    if ".o" not in target or size in ("0x0", "0x00"):
        continue
    n = int(size, 16)
    sizes[target] += n
    fams[family(target)] += n

total = sum(fams.values())
print("== %s : attributed %d bytes" % (path, total))
for name, n in fams.most_common():
    print("%9d %5.1f%%  %s" % (n, 100.0 * n / total, name))
print("-- top objects")
for name, n in sizes.most_common(25):
    print("%9d  %s" % (n, name[-100:]))
