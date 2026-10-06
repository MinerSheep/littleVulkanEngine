#!/usr/bin/env python3
"""Turns a PNG into the raw .tex the engine loads.

    python3 tools/png2tex.py textures/wood_flooring.png
    python3 tools/png2tex.py textures/wood_flooring.png --max 512

The engine has no image loader, so the picture is decoded here and written out
flat:

    "LVETEX01"   8 bytes
    width        uint32 little endian
    height       uint32 little endian
    pixels       width * height * 4 bytes of RGBA, top row first

8 bit RGB and RGBA only, nothing interlaced and nothing paletted. Re-run it after
replacing a picture -- the game reads the .tex, never the .png.
"""

import argparse
import struct
import sys
import zlib

MAGIC = b"LVETEX01"


def fail(message):
    sys.stderr.write("png2tex: " + message + "\n")
    raise SystemExit(1)


def unfilter(kind, line, prev, bpp):
    """Turns one filtered scanline back into plain pixels"""
    if kind == 0:
        return
    if kind == 1:
        for i in range(bpp, len(line)):
            line[i] = (line[i] + line[i - bpp]) & 0xFF
    elif kind == 2:
        for i in range(len(line)):
            line[i] = (line[i] + prev[i]) & 0xFF
    elif kind == 3:
        for i in range(len(line)):
            left = line[i - bpp] if i >= bpp else 0
            line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
    elif kind == 4:
        for i in range(len(line)):
            left = line[i - bpp] if i >= bpp else 0
            up = prev[i]
            corner = prev[i - bpp] if i >= bpp else 0
            guess = left + up - corner
            da, db, dc = abs(guess - left), abs(guess - up), abs(guess - corner)
            if da <= db and da <= dc:
                near = left
            elif db <= dc:
                near = up
            else:
                near = corner
            line[i] = (line[i] + near) & 0xFF
    else:
        fail("row filter {} is not a filter".format(kind))


def read_png(path):
    """Decodes a PNG into width, height and a block of RGBA"""
    with open(path, "rb") as handle:
        data = handle.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        fail(path + " is not a PNG")

    width = height = channels = 0
    chunks = []
    at = 8
    while at + 8 <= len(data):
        length = struct.unpack(">I", data[at:at + 4])[0]
        tag = data[at + 4:at + 8]
        body = data[at + 8:at + 8 + length]
        at += 12 + length

        if tag == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8:
                fail("only 8 bit channels are handled, this one is {}".format(depth))
            if ctype not in (2, 6):
                fail("only RGB and RGBA are handled, colour type is {}".format(ctype))
            if interlace:
                fail("interlaced PNGs are not handled")
            channels = 3 if ctype == 2 else 4
        elif tag == b"IDAT":
            chunks.append(body)
        elif tag == b"IEND":
            break

    if not width or not chunks:
        fail(path + " has no picture in it")

    raw = zlib.decompress(b"".join(chunks))
    stride = width * channels
    rows = bytearray(height * stride)
    prev = bytearray(stride)
    at = 0
    for y in range(height):
        kind = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        unfilter(kind, line, prev, channels)
        rows[y * stride:(y + 1) * stride] = line
        prev = line

    if channels == 4:
        return width, height, bytes(rows)

    # No alpha came in, and the engine wants all four channels
    rgba = bytearray(width * height * 4)
    for i in range(width * height):
        rgba[i * 4:i * 4 + 3] = rows[i * 3:i * 3 + 3]
        rgba[i * 4 + 3] = 255
    return width, height, bytes(rgba)


def shrink(width, height, pixels, longest):
    """Box averages the picture down until its longest side fits"""
    if longest <= 0 or max(width, height) <= longest:
        return width, height, pixels

    scale = float(longest) / max(width, height)
    wide = max(1, int(round(width * scale)))
    tall = max(1, int(round(height * scale)))

    spans = [(x * width // wide, max(x * width // wide + 1, (x + 1) * width // wide))
             for x in range(wide)]

    out = bytearray(wide * tall * 4)
    for y in range(tall):
        top = y * height // tall
        bottom = max(top + 1, (y + 1) * height // tall)
        for x in range(wide):
            left, right = spans[x]
            red = green = blue = alpha = count = 0
            for sy in range(top, bottom):
                at = (sy * width + left) * 4
                for _ in range(left, right):
                    red += pixels[at]
                    green += pixels[at + 1]
                    blue += pixels[at + 2]
                    alpha += pixels[at + 3]
                    at += 4
                    count += 1
            out_at = (y * wide + x) * 4
            out[out_at] = red // count
            out[out_at + 1] = green // count
            out[out_at + 2] = blue // count
            out[out_at + 3] = alpha // count

    return wide, tall, bytes(out)


def main():
    parser = argparse.ArgumentParser(
        description="Turns a PNG into the raw .tex the engine loads")
    parser.add_argument("source")
    parser.add_argument("-o", "--out", help="where the .tex goes, beside the PNG by default")
    parser.add_argument("--max", type=int, default=512, dest="longest",
                        help="longest side in pixels, 0 leaves it at full size")
    args = parser.parse_args()

    width, height, pixels = read_png(args.source)
    width, height, pixels = shrink(width, height, pixels, args.longest)

    out = args.out or args.source.rsplit(".", 1)[0] + ".tex"
    with open(out, "wb") as handle:
        handle.write(MAGIC + struct.pack("<II", width, height) + pixels)
    print("wrote {} {} x {}".format(out, width, height))


if __name__ == "__main__":
    main()
