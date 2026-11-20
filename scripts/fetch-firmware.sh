#!/bin/bash
# Copy the firmware built on the Coder workspace into build-local/, where the Renode scripts
# and the flashing commands in README.md look for it.
. ~/.config/lpp-coder/env
cd "$(dirname "$0")/.." || exit 1
mkdir -p build-local
rsync -a -e coder-rsh \
    prism:prism-stm32/build/stm32h7b3i_dk.elf prism:prism-stm32/build/stm32h7b3i_dk.bin \
    prism:prism-stm32/build/stm32h7b3i_dk_renode.elf build-local/
ls -la build-local
