#!/bin/bash
#
# Build a .deb of nc2000 for CardputerZero.
#
# Usage:  ./packaging/build_deb.sh [version] [arch]
#   version  default: read from packaging/VERSION or fallback 1.0.0
#   arch     default: dpkg --print-architecture  (typically arm64 on CZ)
#
# Produces: build/nc2000_<version>_<arch>.deb
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

VERSION="${1:-$(cat packaging/VERSION 2>/dev/null || echo 1.0.0)}"
ARCH="${2:-$(dpkg --print-architecture)}"

BUILD_DIR="$ROOT_DIR/build"
STAGE_DIR="$BUILD_DIR/stage"
DEB_NAME="nc2000_${VERSION}_${ARCH}.deb"

echo "==> Configuring (version=$VERSION arch=$ARCH)"
rm -rf "$STAGE_DIR"
mkdir -p "$BUILD_DIR"
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "==> Staging to $STAGE_DIR"
# CMAKE install layout uses share/nc2000 + share/APPLaunch/applications+images.
# The reference Debian layout placed everything under /usr/...
cmake --install "$BUILD_DIR" --prefix "$STAGE_DIR/usr"

echo "==> Writing DEBIAN/control"
mkdir -p "$STAGE_DIR/DEBIAN"
cat > "$STAGE_DIR/DEBIAN/control" <<EOF
Package: nc2000
Version: $VERSION
Section: games
Priority: optional
Architecture: $ARCH
Depends: libsdl2-2.0-0
Maintainer: eggfly <eggfly@users.noreply.github.com>
Description: NC2000 WenQuXing Emulator for CardputerZero
 A WenQuXing (文曲星) NC2000 / NC1020 handheld computer emulator
 tuned for M5CardputerZero's 320x170 LCD. Based on wangyu-/NC2000.
EOF

echo "==> Building deb"
cd "$BUILD_DIR"
# dpkg-deb requires ownership root:root; use fakeroot if available.
if command -v fakeroot >/dev/null 2>&1; then
    fakeroot dpkg-deb --build "$STAGE_DIR" "$DEB_NAME"
else
    dpkg-deb --build "$STAGE_DIR" "$DEB_NAME"
fi

echo "==> Result: $BUILD_DIR/$DEB_NAME"
ls -lh "$BUILD_DIR/$DEB_NAME"
