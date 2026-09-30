#!/bin/sh
# Cross-compiles for webOS and builds an .ipk.
#
# Usage: scripts/package.sh <path to arm-webos-linux-gnueabi_sdk-buildroot>
#
# Toolchain: https://github.com/openlgtv/buildroot-nc4/releases
# ares-package: `npm install -g @webos-tools/cli` (or @webosbrew/ares-cli)
set -eu

TOOLCHAIN=${1:?usage: $0 <toolchain dir>}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build-webos"

cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN/share/buildroot/toolchainfile.cmake" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$(nproc)"
"$TOOLCHAIN/bin/arm-webos-linux-gnueabi-strip" "$BUILD/pkg/webos-home-minimal"

ares-package "$BUILD/pkg" -o "$BUILD"
ls "$BUILD"/*.ipk
