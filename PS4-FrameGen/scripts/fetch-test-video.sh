#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p data
target=data/opencv-1920x1080.avi
if [ ! -f "$target" ]; then
    curl --fail --location --retry 2 --output "$target.download" \
        https://raw.githubusercontent.com/opencv/opencv_extra/42a04e79550c8a58a0ec1b41aa2bce25236dbb0e/testdata/cv/video/1920x1080.avi
    mv "$target.download" "$target"
fi
printf '%s  %s\n' 42f77d89c19428a06c70e3c8f455cc3cced40bd0b46dafee3bfbfde323362153 "$target" | sha256sum --check
