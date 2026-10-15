#!/usr/bin/env bash
# Source this file in every fresh shell; it does not install anything.
export FRAMEGEN_TOOLS_ROOT=${FRAMEGEN_TOOLS_ROOT:-/workspace/framegen-toolchains}
export PATH="$FRAMEGEN_TOOLS_ROOT/host-tools/bin:$PATH"
export OO_PS4_TOOLCHAIN="$FRAMEGEN_TOOLS_ROOT/OpenOrbis/PS4Toolchain"
export FRAMEGEN_PS4_CXX="$FRAMEGEN_TOOLS_ROOT/llvm-root/usr/bin/clang++-19"
export FRAMEGEN_PS4_LD="$FRAMEGEN_TOOLS_ROOT/llvm-root/usr/bin/ld.lld-19"
export FRAMEGEN_PKG_LIBDIR="$FRAMEGEN_TOOLS_ROOT/pkg-runtime/usr/lib/x86_64-linux-gnu"
