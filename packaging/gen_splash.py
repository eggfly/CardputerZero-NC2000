#!/usr/bin/env python3
"""Generate a 320x170 splash image (RGB565-friendly) showing the
CardputerZero keymap for NC2000.

The output is packaged as /usr/share/nc2000/splash.png and painted
to /dev/fb0 for a few seconds before the emulator starts.
"""
from PIL import Image, ImageDraw, ImageFont
import os, sys

W, H = 320, 170

# Background: dark blue like NC2000 boot screen
BG = (10, 32, 62)
FG = (245, 245, 245)
HEAD = (255, 220, 110)   # warm yellow for title
SUB  = (140, 200, 255)   # light blue for section

img = Image.new("RGB", (W, H), BG)
d = ImageDraw.Draw(img)

# Try a CJK-capable font on the dev host; fall back to default.
CJK_CANDIDATES = [
    "/System/Library/Fonts/PingFang.ttc",                         # macOS
    "/System/Library/Fonts/STHeiti Medium.ttc",
    "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",     # Debian/Ubuntu
    "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",             # alt
    "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
]
def load(size):
    for p in CJK_CANDIDATES:
        if os.path.exists(p):
            try:
                return ImageFont.truetype(p, size)
            except Exception:
                pass
    return ImageFont.load_default()

f_title = load(16)
f_body  = load(12)

# Title
d.text((8, 4), "NC2000 模拟器 · CardputerZero 键位", font=f_title, fill=HEAD)

# Two-column layout for the 8 function keys
left_col_x = 8
right_col_x = 170
row_y = 28
row_h = 15

L = [
    ("Sym + 1", "英汉"),
    ("Sym + 2", "名片"),
    ("Sym + 3", "计算"),
    ("Sym + 4", "行程"),
]
R = [
    ("Sym + 5", "测验"),
    ("Sym + 6", "时间"),
    ("Sym + 7", "网络"),
    ("Sym + 8", "on/off"),
]
for i, (k, v) in enumerate(L):
    d.text((left_col_x, row_y + i * row_h), k, font=f_body, fill=SUB)
    d.text((left_col_x + 70, row_y + i * row_h), v, font=f_body, fill=FG)
for i, (k, v) in enumerate(R):
    d.text((right_col_x, row_y + i * row_h), k, font=f_body, fill=SUB)
    d.text((right_col_x + 70, row_y + i * row_h), v, font=f_body, fill=FG)

# Separator
d.line([(6, 94), (W - 6, 94)], fill=(60, 90, 130))

# Lower block — other frequently needed keys
foot_y = 100
foot_lines = [
    ("Alt",            "红外"),
    ("; / '",          "发音 / 报时"),
    ("[  ]  \\",       "求助 / 中英数 / 输入法"),
    ("ESC",            "跳出    (长按 3 秒回 Launcher)"),
    ("TAB",            "快进开关"),
]
for i, (k, v) in enumerate(foot_lines):
    d.text((8,  foot_y + i * row_h), k, font=f_body, fill=SUB)
    d.text((80, foot_y + i * row_h), v, font=f_body, fill=FG)

out = sys.argv[1] if len(sys.argv) > 1 else "splash.png"
img.save(out, "PNG")
print("wrote", out, os.path.getsize(out), "bytes")

# Also emit a raw RGB565 version at the same basename.
raw = os.path.splitext(out)[0] + ".rgb565"
buf = bytearray()
for y in range(H):
    for x in range(W):
        r, g, b = img.getpixel((x, y))
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        buf.append(v & 0xFF)
        buf.append((v >> 8) & 0xFF)
with open(raw, "wb") as f:
    f.write(buf)
print("wrote", raw, os.path.getsize(raw), "bytes")
