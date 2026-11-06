# Draws the 3DS Home menu icon (48x48) and banner (256x128) from scratch: dot-matrix lettering, a dotted jump arc and a dotted platform.
# Nothing here comes from the original game. Run: python3 make_art.py  (needs Pillow). Writes icon.png and banner.png next to this file.
import math, os
from PIL import Image, ImageDraw, ImageFilter

G = {
 'P': [0x1e,0x11,0x11,0x1e,0x10,0x10,0x10], 'N': [0x11,0x19,0x15,0x13,0x11,0x11,0x11], 'J': [0x07,0x02,0x02,0x02,0x02,0x12,0x0c],
 'A': [0x0e,0x11,0x11,0x1f,0x11,0x11,0x11], 'C': [0x0e,0x11,0x10,0x10,0x10,0x11,0x0e], 'U': [0x11,0x11,0x11,0x11,0x11,0x11,0x0e],
 'M': [0x11,0x1b,0x15,0x15,0x11,0x11,0x11], '-': [0,0,0,0x1f,0,0,0], "'": [0x04,0x04,0x08,0,0,0,0] }
S = 8   # supersample

def bg(w, h):
    im = Image.new("RGB", (w * S, h * S))
    px = im.load()
    for y in range(h * S):
        t = y / (h * S - 1)
        c = (int(18 + 40 * t), int(14 + 10 * t), int(52 + 46 * t))
        for x in range(w * S): px[x, y] = c
    return im

def dot(d, cx, cy, r, col, hi=True):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=col)
    if hi:
        d.ellipse([cx - r * 0.55, cy - r * 0.6, cx - r * 0.05, cy - r * 0.1], fill=(255, 255, 235))

def text(d, s, x, y, cell, r, col, gap=1.0):
    for ch in s:
        rows = G[ch]
        for ry in range(7):
            for rx in range(5):
                if rows[ry] >> (4 - rx) & 1: dot(d, (x + (rx + .5) * cell) * S, (y + (ry + .5) * cell) * S, r * S, col)
        x += (5 + gap) * cell

def arc(d, w, h, x0, x1, ybase, hgt, n, r):
    for i in range(n):
        t = i / (n - 1)
        x = x0 + (x1 - x0) * t; y = ybase - math.sin(t * math.pi) * hgt
        dot(d, x * S, y * S, r * S * (0.8 + 0.5 * math.sin(t * math.pi)), (255, 160, 60), hi=False)

def floor_dots(d, x0, x1, y, step, r):
    x = x0
    while x <= x1:
        dot(d, x * S, y * S, r * S, (110, 200, 255), hi=False); x += step

def finish(im, w, h, name):
    im = im.filter(ImageFilter.GaussianBlur(S * 0.25)).resize((w, h), Image.LANCZOS)
    im.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), name))

# icon 48x48: PNJ, arc above, platform below
w = h = 48
im = bg(w, h); d = ImageDraw.Draw(im)
arc(d, w, h, 7, 41, 15, 9, 8, 1.2)
text(d, "PNJ", 3.5, 19, 2.35, 1.0, (255, 214, 64), gap=0.9)
floor_dots(d, 5, 43, 42, 3.2, 1.0)
finish(im, w, h, "icon.png")

# banner 256x128: PAC'N-JUMP across the middle
w, h = 256, 128
im = bg(w, h); d = ImageDraw.Draw(im)
arc(d, w, h, 30, 226, 36, 26, 24, 2.8)
text(d, "PAC'N-JUMP", 9, 44, 4.35, 1.95, (255, 214, 64), gap=0.55)
floor_dots(d, 14, 242, 108, 8, 2.4)
finish(im, w, h, "banner.png")
