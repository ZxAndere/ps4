#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=${FRAMEGEN_TOOLS_ROOT:-/workspace/framegen-toolchains}
# Run setup-cloud.sh first for signed Debian repositories; no system packages are installed.
test -f "$root/apt/apt.conf" || { echo 'Run scripts/setup-cloud.sh first' >&2; exit 1; }
mkdir -p "$root/mingw-packages" "$root/mingw"
(
 cd "$root/mingw-packages"
 APT_CONFIG="$root/apt/apt.conf" apt-get download \
   g++-mingw-w64-x86-64-posix=14.2.0-19+27+b1 \
   gcc-mingw-w64-x86-64-posix=14.2.0-19+27+b1 \
   gcc-mingw-w64-base=14.2.0-19+27+b1 \
   gcc-mingw-w64-x86-64-posix-runtime=14.2.0-19+27+b1 \
   mingw-w64-common=12.0.0-5 mingw-w64-x86-64-dev=12.0.0-5 \
   binutils-mingw-w64-x86-64=2.44-3+12+b1
 for package in ./*.deb; do dpkg-deb -x "$package" "$root/mingw"; done
)
source scripts/cloud-env.sh
FRAMEGEN_MINGW_ROOT="$root/mingw" cmake -S . -B build-windows-v03 \
  -DCMAKE_TOOLCHAIN_FILE=scripts/cross-windows.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows-v03 --parallel 4
# Cross compilation does not execute the Windows tests.
