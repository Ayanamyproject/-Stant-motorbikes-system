#!/usr/bin/env python3
"""Minimal PBO packer (uncompressed, with $PBOPREFIX$ header).

Usage: python pbo.py <source_dir> <out.pbo>

Scripts-only DayZ addons do not need binarization, so a plain PBO is enough.
Files listed in SKIP and anything starting with "." are not packed.
"""
import hashlib
import os
import struct
import sys
import time

SKIP = {"$PBOPREFIX$"}


def pack(src, out):
    prefix_file = os.path.join(src, "$PBOPREFIX$")
    prefix = open(prefix_file, encoding="utf-8").read().strip()

    files = []
    for root, dirs, names in os.walk(src):
        dirs[:] = [d for d in dirs if not d.startswith(".")]
        for name in sorted(names):
            if name.startswith("."):
                continue
            full = os.path.join(root, name)
            rel = os.path.relpath(full, src).replace("/", "\\")
            if rel in SKIP:
                continue
            with open(full, "rb") as f:
                files.append((rel, f.read()))
    files.sort(key=lambda item: item[0].lower())

    header = bytearray()
    # product entry with the prefix property
    header += b"\0" + struct.pack("<5I", 0x56657273, 0, 0, 0, 0)
    header += b"prefix\0" + prefix.encode("utf-8") + b"\0" + b"\0"
    stamp = int(time.time())
    for rel, data in files:
        header += rel.encode("utf-8") + b"\0" + struct.pack("<5I", 0, len(data), 0, stamp, len(data))
    header += b"\0" + struct.pack("<5I", 0, 0, 0, 0, 0)

    body = bytes(header) + b"".join(data for _, data in files)
    body += b"\0" + hashlib.sha1(body).digest()

    out_dir = os.path.dirname(os.path.abspath(out))
    os.makedirs(out_dir, exist_ok=True)
    with open(out, "wb") as f:
        f.write(body)
    print(f"{out}: {len(files)} files, {len(body)} bytes, prefix={prefix}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    pack(sys.argv[1], sys.argv[2])
