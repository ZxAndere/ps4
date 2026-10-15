#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to the complete official OpenOrbis SDK}"
compiler=${FRAMEGEN_PS4_CXX:-clang++}
linker=${FRAMEGEN_PS4_LD:-ld.lld}
sdk=$OO_PS4_TOOLCHAIN
destination=${FRAMEGEN_PS4_BUILD_DIR:-$PWD/build-ps4-v03-final}
mkdir -p "$destination/obj" "$destination/package/sce_sys" "$destination/dotnet"
for required in include/orbis/VideoOut.h include/c++/v1/vector lib/libc.a lib/libc++.a lib/crt1.o bin/linux/create-fself bin/linux/PkgTool.Core bin/linux/create-gp4; do
    test -f "$sdk/$required" || { echo "Missing OpenOrbis component: $required" >&2; exit 1; }
done
export DOTNET_BUNDLE_EXTRACT_BASE_DIR="$destination/dotnet"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
sources=(engine/capture/image.cpp engine/motion/block_match.cpp engine/interpolation/interpolate.cpp
    engine/scaling/scale.cpp engine/performance/pipeline.cpp engine/presentation/monitor.cpp
    engine/optimized/processor.cpp engine/performance/runtime.cpp
    overlay/input/controller.cpp overlay/settings/settings.cpp overlay/menu/draw.cpp ps4-app/main.cpp)
objects=()
for source in "${sources[@]}"; do
    object="$destination/obj/${source//\//_}.o"
    "$compiler" --target=x86_64-pc-freebsd12-elf -std=c++17 -O3 -msse2 -fPIC -funwind-tables \
        -D__PS4__ -D_GNU_SOURCE -isysroot "$sdk" -isystem "$sdk/include" -isystem "$sdk/include/c++/v1" \
        -I . -c "$source" -o "$object"
    objects+=("$object")
done
# A cooperative module is inert unless the user explicitly opts into loading it in the native app.
"$compiler" --target=x86_64-pc-freebsd12-elf -std=c++17 -O3 -msse2 -fPIC -funwind-tables \
    -D__PS4__ -D_GNU_SOURCE -DFG_BRIDGE_BUILD -isysroot "$sdk" -isystem "$sdk/include" \
    -isystem "$sdk/include/c++/v1" -I . -c goldhen/plugin/bridge.cpp -o "$destination/obj/bridge.o"
library_objects=()
for object in "${objects[@]}"; do if [[ "$object" != *ps4-app_main.cpp.o ]]; then library_objects+=("$object"); fi; done
"$linker" -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr -e module_start \
    --export-dynamic-symbol=fg_bridge_api --export-dynamic-symbol=plugin_load --export-dynamic-symbol=plugin_unload \
    --export-dynamic-symbol=g_pluginName --export-dynamic-symbol=g_pluginDesc \
    --export-dynamic-symbol=g_pluginAuth --export-dynamic-symbol=g_pluginVersion \
    -L"$sdk/lib" "${library_objects[@]}" "$destination/obj/bridge.o" "$sdk/lib/crtlib.o" \
    --start-group -lc++ -lc -lkernel --end-group -o "$destination/framegen_bridge.elf"
"$sdk/bin/linux/create-fself" -in="$destination/framegen_bridge.elf" -out="$destination/framegen_bridge.oelf" \
    --lib="$destination/package/framegen_bridge.prx" --paid 0x3800000000000011
"$linker" -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
    -L"$sdk/lib" "${objects[@]}" "$sdk/lib/crt1.o" \
    --start-group -lc++ -lc -lkernel -lSceVideoOut -lScePad -lSceUserService --end-group \
    -o "$destination/framegen.elf"
"$sdk/bin/linux/create-fself" -in="$destination/framegen.elf" -out="$destination/framegen.oelf" \
    --eboot "$destination/package/eboot.bin" --paid 0x3800000000000011
tool=(env "LD_LIBRARY_PATH=${FRAMEGEN_PKG_LIBDIR:-}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$sdk/bin/linux/PkgTool.Core")
content=IV0000-FGEN00003_00-PS4FRAMEGEN00003
cd "$destination/package"
"${tool[@]}" sfo_new sce_sys/param.sfo
for key in APP_TYPE ATTRIBUTE DOWNLOAD_DATA_SIZE SYSTEM_VER; do
    value=0; if [ "$key" = APP_TYPE ]; then value=1; fi
    "${tool[@]}" sfo_setentry sce_sys/param.sfo "$key" --type Integer --maxsize 4 --value "$value"
done
for item in 'APP_VER:01.02:8' 'CATEGORY:gd:4' "CONTENT_ID:$content:48" 'TITLE:PS4 FrameGen 0.3:128' 'TITLE_ID:FGEN00003:12' 'VERSION:01.02:8'; do
    IFS=: read -r key value length <<< "$item"
    "${tool[@]}" sfo_setentry sce_sys/param.sfo "$key" --type Utf8 --maxsize "$length" --value "$value"
done
"$sdk/bin/linux/create-gp4" -out project.gp4 --content-id="$content" --files "eboot.bin framegen_bridge.prx sce_sys/param.sfo"
"${tool[@]}" pkg_build project.gp4 "$destination"
"${tool[@]}" pkg_validate "$destination/$content.pkg"
echo "Compiled and package-validated: $destination/$content.pkg (not executed on a PS4)"
