#!/bin/bash
# Sync this repo to the Coder workspace `prism` and run a command there (in ~/prism-stm32),
# with the Arm toolchain on PATH. PRISM itself is the workspace's ~/PRSIM checkout.
. ~/.config/lpp-coder/env
cd "$(dirname "$0")/.." || exit 1
timeout 600 rsync -a --delete --exclude 'build*/' --exclude .git/ \
    --include 'subprojects/packagefiles/***' --exclude 'subprojects/*/' -e coder-rsh ./ prism:prism-stm32/ || exit 1
timeout 1500 coder ssh prism -- "cd ~/prism-stm32 && export PATH=\$HOME/opt/arm-gcc/bin:\$PATH && $1"
