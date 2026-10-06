#!/usr/bin/env python3
"""Turns a preview PPM into a PNG, so a render can be opened on the Windows side.

    python3 tools/ppm2png.py /tmp/yard.ppm /tmp/yard.png
"""

import sys, zlib, struct

src, dst = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    data = f.read()

# P6 header: three whitespace separated fields after the magic
fields, at = [], 2
while len(fields) < 3:
    while data[at:at+1].isspace():
        at += 1
    if data[at:at+1] == b"#":
        while data[at:at+1] != b"\n":
            at += 1
        continue
    start = at
    while not data[at:at+1].isspace():
        at += 1
    fields.append(int(data[start:at]))
at += 1
w, h, _ = fields
pixels = data[at:]

raw = b"".join(b"\x00" + pixels[y*w*3:(y+1)*w*3] for y in range(h))

def chunk(tag, body):
    return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))

png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(raw, 9))
       + chunk(b"IEND", b""))
with open(dst, "wb") as f:
    f.write(png)
print("wrote", dst, w, "x", h)
