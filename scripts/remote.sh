#!/bin/bash
# Sync this repo to the Coder workspace `prism` and run a command there (in ~/prism-stm32),
# with the Arm toolchain on PATH. PRISM itself is the workspace's ~/PRSIM checkout; there,
# subprojects/prism is a symlink to it (ln -sfn ~/PRSIM ~/prism-stm32/subprojects/prism, once),
# which the sync must leave alone: 'subprojects/*/' does not match a symlink, and --delete
# would remove it.
. ~/.config/lpp-coder/env
cd "$(dirname "$0")/.." || exit 1
timeout 600 rsync -a --delete --exclude 'build*/' --exclude .git/ --exclude /subprojects/prism \
    --include 'subprojects/packagefiles/***' --exclude 'subprojects/*/' -e coder-rsh ./ prism:prism-stm32/ || exit 1
timeout 1500 coder ssh prism -- "cd ~/prism-stm32 && export PATH=\$HOME/opt/arm-gcc-threads/bin:\$PATH && $1"
