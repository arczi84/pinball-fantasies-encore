#!/usr/bin/env python3
"""A Workbench icon (.info) as AmigaOS 4 / AfA OS / OS 3.2 read them: a PNG picture with an
'icOn' chunk of attributes (the format of the PNG icons OpenLara-Maggie3D ships).

  make_icon.py <picture.png> <out.info> [--tool C:IconX] [--type project|tool] [--stack N] [--tooltype T ...]
"""
import argparse
import io
import struct
import zlib

from PIL import Image

ATTR_DRAWERX, ATTR_DRAWERY, ATTR_DRAWERWIDTH, ATTR_DRAWERHEIGHT = 0x80001003, 0x80001004, 0x80001005, 0x80001006
ATTR_DRAWERFLAGS, ATTR_STACKSIZE, ATTR_DEFAULTTOOL, ATTR_TOOLTYPE = 0x80001007, 0x80001009, 0x8000100A, 0x8000100B
ATTR_TYPE = 0x8000100F
TYPES = {"drawer": 2, "tool": 3, "project": 4}


def chunk(kind: bytes, body: bytes) -> bytes:
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def main() -> None:
    a = argparse.ArgumentParser()
    a.add_argument("picture")
    a.add_argument("out")
    a.add_argument("--size", type=int, default=64)
    a.add_argument("--tool", default="")
    a.add_argument("--type", default="project", choices=TYPES)
    a.add_argument("--stack", type=int, default=200000)
    a.add_argument("--tooltype", action="append", default=[])
    o = a.parse_args()

    img = Image.open(o.picture).convert("RGBA").resize((o.size, o.size), Image.LANCZOS)
    png = io.BytesIO()
    img.save(png, "PNG", optimize=True)
    data = png.getvalue()

    attrs = struct.pack(">II", ATTR_TYPE, TYPES[o.type])
    if o.type == "drawer":
        # (where the drawer's window opens, and its size)
        attrs += struct.pack(">IIIIIIII", ATTR_DRAWERX, 40, ATTR_DRAWERY, 40, ATTR_DRAWERWIDTH, 480, ATTR_DRAWERHEIGHT, 260)
    else:
        attrs += struct.pack(">II", ATTR_STACKSIZE, o.stack)
    if o.tool:
        attrs += struct.pack(">I", ATTR_DEFAULTTOOL) + o.tool.encode("latin-1") + b"\0"
    for t in o.tooltype:
        attrs += struct.pack(">I", ATTR_TOOLTYPE) + t.encode("latin-1") + b"\0"

    iend = data.rindex(b"IEND") - 4  # (the length before the type)
    with open(o.out, "wb") as f:
        f.write(data[:iend] + chunk(b"icOn", attrs) + data[iend:])


if __name__ == "__main__":
    main()
