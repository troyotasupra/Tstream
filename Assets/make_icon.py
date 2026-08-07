"""Generates Assets/icon.png, the Tstream application icon.

Written against the standard library only (zlib + struct are enough to emit a
valid PNG) because neither Pillow nor ImageMagick is installed on the build
machine, and the icon is not worth a new dependency.

Palette is taken verbatim from TstreamColours in Source/PluginEditor.h so the
icon and the UI cannot drift apart:
    background #1D1D1C   highlight #DFFF00   text #01F7F7

Regenerate with:  python Assets/make_icon.py
"""
import math
import struct
import zlib

SIZE = 512
BG = (0x1D, 0x1D, 0x1C)
HIGHLIGHT = (0xDF, 0xFF, 0x00)
CYAN = (0x01, 0xF7, 0xF7)


def rounded_alpha(x, y, size, radius):
    """Squircle-ish corner mask, antialiased over one pixel."""
    cx = min(x, size - 1 - x)
    cy = min(y, size - 1 - y)
    if cx >= radius or cy >= radius:
        return 1.0
    dx = radius - cx
    dy = radius - cy
    d = math.hypot(dx, dy)
    return max(0.0, min(1.0, radius - d + 0.5))


def blend(dst, src, a):
    return tuple(int(round(d + (s - d) * a)) for d, s in zip(dst, src))


def coverage(dist, half):
    """1 inside the shape, 0 outside, antialiased across a one-pixel edge."""
    return max(0.0, min(1.0, half - dist + 0.5))


def build():
    px = [[(0, 0, 0, 0)] * SIZE for _ in range(SIZE)]

    # Geometry, in units of the canvas so the design scales with SIZE.
    bar_y = SIZE * 0.30          # top bar of the T
    bar_half_h = SIZE * 0.052
    bar_half_w = SIZE * 0.26
    stem_half_w = SIZE * 0.052
    stem_bottom = SIZE * 0.60

    wave_y = SIZE * 0.76
    wave_amp = SIZE * 0.10
    wave_half = SIZE * 0.030
    wave_x0 = SIZE * 0.13
    wave_x1 = SIZE * 0.87

    for y in range(SIZE):
        row = px[y]
        for x in range(SIZE):
            a = rounded_alpha(x, y, SIZE, SIZE * 0.18)
            if a <= 0.0:
                continue

            col = BG

            # --- T glyph, cyan ---
            dx = abs(x - SIZE / 2.0)
            in_bar = coverage(abs(y - bar_y), bar_half_h) * coverage(dx, bar_half_w)
            in_stem = 0.0
            if y >= bar_y:
                in_stem = coverage(dx, stem_half_w) * coverage(
                    max(0.0, y - stem_bottom), 0.5)
            t_cov = max(in_bar, in_stem)
            if t_cov > 0.0:
                col = blend(col, CYAN, t_cov)

            # --- signal wave, highlight yellow ---
            if wave_x0 <= x <= wave_x1:
                phase = (x - wave_x0) / (wave_x1 - wave_x0)
                wy = wave_y + math.sin(phase * 2.0 * math.pi * 1.5) * wave_amp
                w_cov = coverage(abs(y - wy), wave_half)
                if w_cov > 0.0:
                    col = blend(col, HIGHLIGHT, w_cov)

            row[x] = (col[0], col[1], col[2], int(round(255 * a)))

    return px


def write_png(path, px):
    raw = bytearray()
    for row in px:
        raw.append(0)  # filter type 0 (None) for every scanline
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")

    with open(path, "wb") as f:
        f.write(png)


if __name__ == "__main__":
    import os
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon.png")
    write_png(out, build())
    print("wrote", out, os.path.getsize(out), "bytes")
