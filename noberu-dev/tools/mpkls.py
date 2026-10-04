#!/usr/bin/env python3
"""List an MPK v2.0 archive (same layout as src/io/mpkarchive.cpp).

  mpkls.py ARCHIVE.mpk            id, compressed?, size, name per entry
  mpkls.py ARCHIVE.mpk -x ID OUT  extract one entry (inflates zlib entries)
"""
import struct, sys, zlib

def entries(path):
    with open(path, "rb") as f:
        hdr = f.read(16)
        assert hdr[:4] == b"MPK\0", "not an MPK"
        minor, major, count = struct.unpack_from("<HHI", hdr, 4)
        assert (major, minor) == (2, 0), f"MPK {major}.{minor}"
        f.seek(0x40)
        for _ in range(count):
            comp, eid, off, csize, size = struct.unpack("<IIQQQ", f.read(32))
            name = f.read(224).split(b"\0", 1)[0].decode("utf-8", "replace")
            if not off:
                break
            yield eid, comp, off, csize, size, name

def main():
    path = sys.argv[1]
    if len(sys.argv) >= 5 and sys.argv[2] == "-x":
        want = int(sys.argv[3], 0)
        for eid, comp, off, csize, size, name in entries(path):
            if eid == want:
                with open(path, "rb") as f:
                    f.seek(off)
                    data = f.read(csize)
                if comp:
                    data = zlib.decompress(data)
                open(sys.argv[4], "wb").write(data)
                print(f"{name}: {len(data)} bytes")
                return
        sys.exit(f"no entry {want}")
    for eid, comp, off, csize, size, name in entries(path):
        print(f"{eid:5d} {'z' if comp else '-'} {size:11d}  {name}")

main()
