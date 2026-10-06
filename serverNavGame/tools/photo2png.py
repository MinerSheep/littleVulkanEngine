#!/usr/bin/env python3
"""Turns a filed photograph into a PNG you can look at.

    python3 tools/photo2png.py saves/photos/photo_0001.txt out.png [zoom]

The photograph is rows of six-digit hex, which is what the game files. Zoom
repeats each pixel, since 64 x 48 is small on a modern screen.
"""
import sys, zlib, struct

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    zoom = int(sys.argv[3]) if len(sys.argv) > 3 else 8

    rows = []
    for line in open(src, encoding='utf-8'):
        parts = line.split()
        if parts and parts[0] == 'px':
            rows.append([bytes.fromhex(p) for p in parts[1:]])
    if not rows:
        print('no pixels in', src)
        return 1

    wide, tall = len(rows[0]), len(rows)
    raw = b''
    for row in rows:
        line = b''.join(dot * zoom for dot in row)
        raw += (b'\x00' + line) * zoom

    def chunk(tag, data):
        body = tag + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body))

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', wide * zoom, tall * zoom, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw))
           + chunk(b'IEND', b''))
    open(dst, 'wb').write(png)
    print('%s -> %s  (%d x %d, zoom %d)' % (src, dst, wide, tall, zoom))
    return 0

sys.exit(main())
