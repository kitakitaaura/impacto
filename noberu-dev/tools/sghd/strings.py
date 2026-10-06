#!/usr/bin/env python3
"""Print an SCX string table, decoding Latin glyphs (Steam S;G font layout).

  strings.py FILE.SCX [FIRST [COUNT]]
Glyphs: 128-137 digits, 138-163 A-Z, 164-189 a-z, 63 space, 201 apostrophe;
others print as {id}, control bytes as <xx>.
"""
import struct, sys

path = sys.argv[1]
first = int(sys.argv[2]) if len(sys.argv) > 2 else 0
count = int(sys.argv[3]) if len(sys.argv) > 3 else 10**9
d = open(path, "rb").read()
str_off, ret_off = struct.unpack_from("<II", d, 4)
offs = [struct.unpack_from("<I", d, o)[0] for o in range(str_off, ret_off, 4)]

def glyph(i):
    if 128 <= i <= 137: return chr(48 + i - 128)
    if 138 <= i <= 163: return chr(65 + i - 138)
    if 164 <= i <= 189: return chr(97 + i - 164)
    if i == 63 or i == 0: return " "
    if i == 201: return "'"
    return "{%d}" % i

def decode(p):
    out = []
    while p < len(d):
        b = d[p]
        if b == 0xFF: break
        if b & 0x80:
            out.append(glyph(((b & 0x7F) << 8) | d[p + 1])); p += 2
        else:
            out.append("<%02x>" % b); p += 1
    return "".join(out)

for i in range(first, min(len(offs), first + count)):
    print(i, decode(offs[i]))
