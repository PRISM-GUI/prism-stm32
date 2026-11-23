#!/bin/bash
# Copy the firmware built on the Coder workspace into build-local/, where the Renode scripts
# and the flashing commands in README.md look for it.
# Usage: fetch-firmware.sh [board] [build-dir]   (defaults: stm32h7b3i_dk build)
. ~/.config/lpp-coder/env
cd "$(dirname "$0")/.." || exit 1
board="${1:-stm32h7b3i_dk}"
build_dir="${2:-build}"
mkdir -p build-local
rsync -a -e coder-rsh \
    "prism:prism-stm32/$build_dir/$board.elf" \
    "prism:prism-stm32/$build_dir/$board.bin" \
    "prism:prism-stm32/$build_dir/${board}_renode.elf" build-local/
ls -la build-local
