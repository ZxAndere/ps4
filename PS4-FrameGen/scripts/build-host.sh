#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
target=${FRAMEGEN_HOST_BUILD_DIR:-build-v03}
cmake -S . -B "$target" -DCMAKE_BUILD_TYPE=Release
cmake --build "$target" --parallel 4
ctest --test-dir "$target" --output-on-failure
