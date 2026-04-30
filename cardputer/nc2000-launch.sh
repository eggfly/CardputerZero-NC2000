#!/bin/sh
# NC2000 launcher for CardputerZero.
#
# Matches the legacy behavior (no pixel-size overrides) since that's
# what is confirmed to start correctly via APPLauncher. The resulting
# window is bigger than 320x170 and will get cropped by the framebuffer,
# but the LCD content area is still visible.
cd /usr/share/nc2000
exec ./nc2000 "$@"
