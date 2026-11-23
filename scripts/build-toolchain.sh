#!/bin/bash
# An arm-none-eabi GCC whose libstdc++ has threads (std::thread, std::mutex, atomic shared_ptr),
# for the Cortex-M7 only. Runs on the Coder workspace (~1 h on 16 cores).
#
# Reuses the xPack toolchain's binutils and newlib (~/opt/arm-gcc); rebuilds GCC with
# --enable-threads=posix. newlib's <pthread.h> is only declared when _POSIX_THREADS is
# defined, so the target libraries are built with it; the pthread_* functions themselves
# come later, from FreeRTOS (FreeRTOS-Plus-POSIX or our own layer).
# simplify: one library variant per toolchain instead of xPack's multilib set. FPU picks it:
# fpv5-d16 (double precision, the H7, the default, in ~/opt/arm-gcc-threads) or fpv5-sp-d16
# (single precision, the F7: FPU=fpv5-sp-d16 PREFIX=~/opt/arm-gcc-threads-sp
# WORK=~/toolchain-build-sp). Libraries built for the wrong one run double-precision FPU
# instructions the F7 does not have: an undefined-instruction fault in log10 on the board.
set -euo pipefail

V=${V:-15.2.0}
XPACK=${XPACK:-$HOME/opt/arm-gcc}
PREFIX=${PREFIX:-$HOME/opt/arm-gcc-threads}
FPU=${FPU:-fpv5-d16}
case "$FPU" in
    fpv5-d16) NEWLIB_VARIANT=v7e-m+dp ;;
    fpv5-sp-d16) NEWLIB_VARIANT=v7e-m+fp ;;
    *) echo "FPU must be fpv5-d16 or fpv5-sp-d16" >&2; exit 1 ;;
esac
WORK=${WORK:-$HOME/toolchain-build}
# The Coder image has no diffutils: GCC's build needs cmp.
command -v cmp >/dev/null || export PATH="$(cd "$(dirname "$0")" && pwd)/cmp-shim:$PATH"
TARGET_FLAGS="-O2 -ffunction-sections -fdata-sections"
# newlib declares pthreads only behind these. Baked into the driver (--with-specs) so every
# compile sees them: the target libraries here, and any program including <mutex> or <thread>.
# The *_clock* pthread calls are GNU extensions newlib declares only under _GNU_SOURCE, which
# libstdc++'s own build defines and programs do not: told absent, so the headers use the plain
# timed calls.
POSIX_SPECS="-D_POSIX_THREADS -D_UNIX98_THREAD_MUTEX_ATTRIBUTES -D_POSIX_TIMEOUTS -D_POSIX_READER_WRITER_LOCKS -D_POSIX_MONOTONIC_CLOCK"

fetch_gcc() {
    mkdir -p "$WORK" && cd "$WORK"
    [ -d gcc-$V ] && return
    curl -sL https://ftp.gnu.org/gnu/gcc/gcc-$V/gcc-$V.tar.xz | tar xJ
    (cd gcc-$V && ./contrib/download_prerequisites)
}

# The xPack tree minus its compiler, with the FPU's hard-float newlib at the sysroot's root
# (a --disable-multilib GCC only looks there).
prepare_prefix() {
    rm -rf "$PREFIX" && cp -a "$XPACK" "$PREFIX"
    rm -rf "$PREFIX/lib/gcc" "$PREFIX/libexec/gcc" "$PREFIX/arm-none-eabi/include/c++"
    rm -f "$PREFIX"/bin/arm-none-eabi-{gcc,g++,c++,cpp,gcov,lto-dump}*
    local lib=$PREFIX/arm-none-eabi/lib
    rm -f "$lib"/libstdc++*.a "$lib"/libsupc++*.a
    cp -f "$lib"/thumb/$NEWLIB_VARIANT/hard/*.a "$lib"/thumb/$NEWLIB_VARIANT/hard/*.o "$lib"/
}

build_gcc() {
    # Read by libstdc++'s configure, which make runs later: exported, not configure arguments.
    export glibcxx_cv_PTHREAD_COND_CLOCKWAIT=no glibcxx_cv_PTHREAD_MUTEX_CLOCKLOCK=no \
           glibcxx_cv_PTHREAD_RWLOCK_CLOCKLOCK=no
    rm -rf "$WORK/build" && mkdir -p "$WORK/build" && cd "$WORK/build"
    ../gcc-$V/configure --target=arm-none-eabi --prefix="$PREFIX" \
        --with-sysroot="$PREFIX/arm-none-eabi" --with-native-system-header-dir=/include \
        --with-build-time-tools="$PREFIX/arm-none-eabi/bin" \
        --disable-multilib --with-cpu=cortex-m7 --with-fpu="$FPU" --with-float=hard --with-mode=thumb \
        --enable-languages=c,c++ --with-newlib --enable-threads=posix --with-specs="$POSIX_SPECS" \
        --disable-shared --disable-nls --disable-libssp --disable-libgomp --disable-libquadmath \
        --disable-libstdcxx-pch \
        CFLAGS_FOR_TARGET="$TARGET_FLAGS" CXXFLAGS_FOR_TARGET="$TARGET_FLAGS" MAKEINFO=true \
        CXX="g++ -std=gnu++17"   # GCC 15 predates the host GCC 16 C++20 default (char8_t); libcody adds its own -std=c++11 after this
    make -j"$(nproc)" MAKEINFO=true
    make install MAKEINFO=true
}

fetch_gcc
prepare_prefix
build_gcc
"$PREFIX/bin/arm-none-eabi-g++" --version | head -1
echo TOOLCHAIN-DONE
