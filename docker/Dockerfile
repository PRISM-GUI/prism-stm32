# A development image for this repository: the cross toolchain the firmware needs (arm-none-eabi
# GCC whose libstdc++ has threads), QEMU to run the test firmwares without the board, openocd
# and stlink, and the host build tools (meson, ninja, GCC 16 for PRISM's -freflection).
#
#   podman build -f docker/Dockerfile -t prism-stm32-dev .     # from the repository root
#
# BASE: any Fedora 44 image. The lab's Coder workspace image builds from it unchanged (its
# packages are already there, so the first dnf step does nothing).
# simplify: Arm Cortex-M7 only; an ESP32 (ESP-IDF) stage is the next block to add.
ARG BASE=registry.fedoraproject.org/fedora:44

# An arm-none-eabi GCC whose libstdc++ has threads (std::thread, std::mutex, atomic shared_ptr),
# built over the xPack toolchain's binutils and newlib. Its own stage: ~20 min on 16 cores.
FROM ${BASE} AS arm-threads
USER root
RUN dnf install -y --setopt=install_weak_deps=False \
        gcc gcc-c++ make tar xz curl diffutils m4 bzip2 patch \
    && dnf clean all
ARG XPACK_VERSION=15.2.1-1.1
RUN mkdir -p /opt && cd /opt \
    && curl -sL https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v${XPACK_VERSION}/xpack-arm-none-eabi-gcc-${XPACK_VERSION}-linux-x64.tar.gz | tar xz \
    && mv xpack-arm-none-eabi-gcc-${XPACK_VERSION} arm-gcc
COPY scripts/build-toolchain.sh /tmp/
RUN XPACK=/opt/arm-gcc PREFIX=/opt/arm-gcc-threads WORK=/tmp/toolchain-build bash /tmp/build-toolchain.sh \
    && rm -rf /tmp/toolchain-build

FROM ${BASE}
USER root
# Host side: meson/ninja/GCC for the host tests and PRISM, git for the submodules.
# Board side: qemu-system-arm (mps2-an500 is a Cortex-M7), openocd and stlink (flash and debug
# over the ST-LINK; a container has no USB unless given one), gdb.
RUN dnf install -y --setopt=install_weak_deps=False \
        gcc gcc-c++ make git meson ninja-build python3 ccache curl tar xz \
        diffutils m4 bzip2 patch flex bison texinfo \
        qemu-system-arm-core openocd stlink gdb \
    && dnf clean all
COPY --from=arm-threads /opt/arm-gcc /opt/arm-gcc
COPY --from=arm-threads /opt/arm-gcc-threads /opt/arm-gcc-threads
# The threaded toolchain first: it is the one a PRISM firmware needs. The stock xPack one
# (multilib for every Cortex-M, gdb) stays under /opt/arm-gcc/bin.
ENV PATH=/opt/arm-gcc-threads/bin:$PATH

USER 1000
