#!/bin/sh
# NC2000 launcher for CardputerZero.
#
# Uses the framebuffer port (nc2000_fb) — it renders /dev/fb0 directly
# and reads /dev/input/event* for keys. The SDL2-linked binary
# (./nc2000) is also shipped in the same dir for headless/dev use, but
# libsdl2 2.32 on Debian trixie has no fbcon/fbdev video driver and
# the CardputerZero LCD is fbtft (not KMS/DRM), so on-device that
# binary can't paint the screen.
cd /usr/share/nc2000
# Default ROM is roms/nc2000  (matches nc2000_fb's built-in default)
exec ./nc2000_fb "$@"
