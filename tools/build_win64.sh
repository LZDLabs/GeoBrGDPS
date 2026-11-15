#!/bin/bash
# Windows x64 build from Linux: clang-cl 19 + lld-link + MSVC CRT / Windows SDK
# from xwin (see /opt/tc/win/toolchain.cmake). Same staging rule as Android:
# only the dependencies declared in the mod's own mod.json.
# Usage: tools/build_win64.sh <source dir> <build dir>
set -euo pipefail
SRC=$(realpath "$1"); BUILD=$(realpath -m "$2")
: "${GEODE_SDK:?set GEODE_SDK}"
DEPS=${GEODE_DEPS_WIN:-/opt/tc/geode-deps-win}
rm -rf "$BUILD/geode-deps"
mkdir -p "$BUILD/geode-deps"
for dep in $(python3 -c 'import json,sys; print(" ".join(json.load(open(sys.argv[1])).get("dependencies") or {}))' "$SRC/mod.json"); do
  if [ ! -d "$DEPS/$dep" ]; then echo "dependency $dep is not staged in $DEPS" >&2; exit 1; fi
  cp -r "$DEPS/$dep" "$BUILD/geode-deps/"
done
cmake -S "$SRC" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=/opt/tc/win/toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
  -DSKIP_BUILDING_CODEGEN=OFF -DGEODE_DONT_USE_CCACHE=ON -DGEODE_DONT_INSTALL_MODS=ON \
  ${CPM_SOURCE_CACHE:+-DCPM_SOURCE_CACHE=$CPM_SOURCE_CACHE}
ninja -C "$BUILD" -k 0
