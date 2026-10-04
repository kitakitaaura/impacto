#!/usr/bin/env python3
"""Linear SGHD disassembly with raw bytes and decoded expressions.

  dis.py FILE.SCX START [COUNT]     (START in hex)
Labels are resolved from the SCX label table: '<- Ln' marks label starts.
"""
import os, struct, subprocess, sys

path, start = sys.argv[1], int(sys.argv[2], 16)
count = int(sys.argv[3]) if len(sys.argv) > 3 else 40
data = open(path, "rb").read()
tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sghdlen")
proc = subprocess.Popen([tool], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

# SCX: "SC3\0", string table offset, return table offset, then label offsets.
str_off = struct.unpack_from("<I", data, 4)[0]
labels = {}
for i, off in enumerate(range(12, str_off, 4)):
    labels.setdefault(struct.unpack_from("<I", data, off)[0], i)

def expr(p):
    """Decode one SC3 expression to a readable token list; return (text, end)."""
    out = []
    names = {0x28: "SW", 0x29: "FLAG", 0x2A: "DATA", 0x2D: "TV", 0x33: "RND"}
    ops = {1: "*", 2: "/", 3: "+", 4: "-", 5: "%", 6: "<<", 7: ">>", 8: "&", 9: "^",
           0xA: "|", 0xC: "==", 0xD: "!=", 0xE: "<=", 0xF: ">=", 0x10: "<", 0x11: ">",
           0x14: "=", 0x17: "+=", 0x18: "-="}
    while data[p]:
        t = data[p]
        if t & 0x80:
            k = (t & 0x60) >> 5
            if k == 0: v = t & 0x1F; v = v - 32 if t & 0x10 else v; n = 1
            elif k == 1: v = ((t & 0x1F) << 8) + data[p+1]; n = 2
            elif k == 2: v = ((t & 0x1F) << 16) + (data[p+2] << 8) + data[p+1]; n = 3
            else: v = struct.unpack_from("<i", data, p+1)[0]; n = 5
            out.append(str(v)); p += n + 1
        else:
            out.append(names.get(t, ops.get(t, f"op{t:02x}"))); p += 2
    return " ".join(out), p + 1

p = start
for _ in range(count):
    if p >= len(data): break
    proc.stdin.write(f"{path} {p:x}\n"); proc.stdin.flush()
    _, n, name = proc.stdout.readline().strip().split(" ", 2)
    n = int(n)
    raw = data[p:p+n]
    note = f"  <- L{labels[p]}" if p in labels else ""
    # Show decoded expressions for anything after the 2-byte opcode.
    exprs, q = [], p + 2
    if name not in ("__Unrecognized__",) and raw[0] != 0xFE:
        while q < p + n and data[q] & 0x80 == 0 and data[q] in (0x28, 0x29, 0x2A, 0x2D):
            e, q = expr(q); exprs.append(f"[{e}]")
    elif raw[0] == 0xFE:
        e, _ = expr(p + 1); exprs.append(f"[{e}]")
    print(f"{p:06x} {name:26s} {raw.hex():40s} {' '.join(exprs)}{note}")
    p += n
