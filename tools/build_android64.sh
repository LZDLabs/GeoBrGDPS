#!/bin/bash
# Reproducible Android arm64 build used for RC6 (see BUILD-RC6.md).
# Requires: NDK r28c, Geode SDK 5.9.0 (+ bindings 7f6c2a75), staged geode-deps.
# Usage: tools/build_android64.sh <source dir> <build dir>
set -euo pipefail
SRC=$(realpath "$1"); BUILD=$(realpath -m "$2")
: "${NDK:?set NDK}" "${GEODE_SDK:?set GEODE_SDK}" "${GEODE_DEPS_TEMPLATE:?set GEODE_DEPS_TEMPLATE}"
# Geode links every required library found in <build>/geode-deps, so only the
# dependencies declared in this mod's own mod.json are staged. (A mod linked
# against an undeclared mod fails to load on the device: "library
# <id>.android64.so not found".)
rm -rf "$BUILD/geode-deps"
mkdir -p "$BUILD/geode-deps"
for dep in $(python3 -c 'import json,sys; print(" ".join(json.load(open(sys.argv[1])).get("dependencies") or {}))' "$SRC/mod.json"); do
  if [ ! -d "$GEODE_DEPS_TEMPLATE/$dep" ]; then echo "dependency $dep is not staged in $GEODE_DEPS_TEMPLATE" >&2; exit 1; fi
  cp -r "$GEODE_DEPS_TEMPLATE/$dep" "$BUILD/geode-deps/"
done
cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-23 -DCMAKE_BUILD_TYPE=Release \
  -DSKIP_BUILDING_CODEGEN=OFF -DGEODE_DONT_USE_CCACHE=ON -DGEODE_DONT_INSTALL_MODS=ON \
  ${CPM_SOURCE_CACHE:+-DCPM_SOURCE_CACHE=$CPM_SOURCE_CACHE}
ninja -C "$BUILD"
