#!/bin/sh
# NC2000 launcher tuned for CardputerZero's 320x170 LCD.
#
# Native LCD of NC2000 is 160x80 + side icon strip (21px left + 7px right - 1).
# With --pixel-size 1 --gap-size 0 --lcd-scale 1 the whole window is 187x80,
# which fits centered on 320x170 with room for the icon strip.
set -e
cd /usr/share/nc2000
exec ./nc2000 \
    --pixel-size 1 \
    --gap-size 0 \
    --lcd-scale 1 \
    "$@"
