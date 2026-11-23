#!/usr/bin/env python3
"""Biggest input sections (one symbol or one table each, thanks to -ffunction/-fdata-sections) and
their reference chains, from the same stripped, cref-enabled map that scripts/size-map.py reads:

  python3 scripts/size-sections.py /tmp/map.txt

Requires a `-Wl,-S -Wl,--cref -Wl,-Map=…` link; see scripts/size-map.py for the recipe.
"""

import collections
import re
import sys

path = sys.argv[1]
lines = open(path, errors="ignore").read().splitlines()
start = max(
    i for i, l in enumerate(lines) if l.startswith("Linker script and memory map")
)

section_re = re.compile(r"^ (\.[^ \t]+)")
data_re = re.compile(r"^\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(\S.*)$")

rows = []
current = "?"
for line in lines[start:]:
    m = section_re.match(line)
    if m:
        current = m.group(1)
        continue
    m = data_re.match(line)
    if not m:
        continue
    size, target = m.group(2), m.group(3).strip()
    if ".o" not in target or size in ("0x0", "0x00"):
        continue
    rows.append((int(size, 16), current, target))

rows.sort(reverse=True)
print("== biggest input sections")
for size, section, target in rows[:20]:
    print("%7d %-48s %s" % (size, section[:48], target[-60:]))

groups = collections.Counter()
for size, section, _ in rows:
    groups[section.split(".")[1] if section.startswith(".") else section] += size
print("== section-name groups")
for name, size in groups.most_common(12):
    print("%8d %s" % (size, name))

try:
    cref_start = next(
        i for i, l in enumerate(lines) if l.startswith("Cross Reference Table")
    )
except StopIteration:
    sys.exit()

refs = collections.defaultdict(list)
current = None
for line in lines[cref_start + 2 :]:
    parts = line.split()
    if len(parts) >= 2:
        current = parts[0]
        refs[current] = [parts[1]]
    elif len(parts) == 1 and current:
        refs[current].append(parts[0])

print("== reference chains")
for size, section, _ in rows[:10]:
    name = section.split(".", 2)[-1] if section.count(".") >= 2 else section
    print("%7d %s" % (size, name[:52]))
    for referrer in (refs.get(name) or ["(no cross-reference entry)"])[:3]:
        print("          <- %s" % referrer[-70:])
