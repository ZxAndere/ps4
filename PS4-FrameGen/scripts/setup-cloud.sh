#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=${FRAMEGEN_TOOLS_ROOT:-/workspace/framegen-toolchains}
mkdir -p "$root"
if [ ! -x "$root/host-tools/bin/cmake" ]; then
    python3 -m venv "$root/host-tools"
    "$root/host-tools/bin/python" -m pip install --no-cache-dir --disable-pip-version-check cmake==3.31.6 ninja==1.11.1.4
fi
if ! "$root/host-tools/bin/python" -c 'import numpy, PIL' >/dev/null 2>&1; then
    "$root/host-tools/bin/python" -m pip install --no-cache-dir --disable-pip-version-check -r scripts/requirements-validation.txt
fi
archive="$root/toolchain-llvm-18.tar.gz"
if [ ! -f "$archive" ]; then
    curl --fail --location --retry 2 --output "$archive.download" \
        https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz
    mv "$archive.download" "$archive"
fi
printf '%s  %s\n' 3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526 "$archive" | sha256sum --check --status
if [ ! -f "$root/OpenOrbis/PS4Toolchain/lib/libc.a" ]; then
    tar -xzf "$archive" -C "$root"
fi
if [ ! -x "$root/llvm-root/usr/bin/clang++-19" ] || [ ! -f "$root/pkg-runtime/usr/lib/x86_64-linux-gnu/libssl.so.1.1" ]; then
    mkdir -p "$root/apt/empty" "$root/apt/lists/partial" "$root/apt/cache" "$root/llvm-root" "$root/pkg-runtime"
    cat > "$root/apt/sources.list" <<'EOF'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian bullseye main
EOF
    cat > "$root/apt/apt.conf" <<EOF
Dir::Etc::Parts "$root/apt/empty";
Dir::Etc::sourcelist "$root/apt/sources.list";
Dir::Etc::sourceparts "$root/apt/empty";
Dir::State::lists "$root/apt/lists";
Dir::Cache "$root/apt/cache";
APT::Sandbox::User "agent";
APT::Update::Error-Mode "any";
EOF
    (
        cd "$root/apt"
        APT_CONFIG="$root/apt/apt.conf" apt-get update
        APT_CONFIG="$root/apt/apt.conf" apt-get download \
            clang-19=1:19.1.7-3+b1 lld-19=1:19.1.7-3+b1 libclang-cpp19=1:19.1.7-3+b1 \
            libclang1-19=1:19.1.7-3+b1 libclang-common-19-dev=1:19.1.7-3+b1 llvm-19-linker-tools=1:19.1.7-3+b1 \
            libssl1.1=1.1.1w-0+deb11u1
        for package in clang-19_*.deb lld-19_*.deb libclang-*.deb libclang1-19_*.deb llvm-19-linker-tools_*.deb; do
            dpkg-deb -x "$package" "$root/llvm-root"
        done
        for package in libssl1.1_*.deb; do dpkg-deb -x "$package" "$root/pkg-runtime"; done
    )
fi
# This Debian image already supplies libLLVM19, GCC, make, Python and FFmpeg.
export FRAMEGEN_TOOLS_ROOT="$root"
source scripts/cloud-env.sh
"$FRAMEGEN_PS4_CXX" --version
cmake --version
bash scripts/build-host.sh
bash scripts/build-ps4.sh
