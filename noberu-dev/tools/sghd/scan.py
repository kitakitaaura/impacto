#!/usr/bin/env python3
"""Find every instruction with a given opcode across SCX scripts.

  scan.py SCRIPTS_DIR GG:OO [GG:OO ...]   e.g. scan.py scripts 10:37
Walks each label linearly with sghdlen (stops at unrecognized bytes) and prints
script, address, label, SGHD name and raw bytes.
"""
import os, struct, subprocess, sys

scripts, wanted = sys.argv[1], {tuple(int(x, 16) for x in a.split(":")) for a in sys.argv[2:]}
tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sghdlen")
proc = subprocess.Popen([tool], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

for name in sorted(os.listdir(scripts)):
    if not name.upper().endswith(".SCX"):
        continue
    path = os.path.join(scripts, name)
    data = open(path, "rb").read()
    str_off = struct.unpack_from("<I", data, 4)[0]
    # The label table runs until the first label's code (not to the string table).
    label_end = min(str_off, struct.unpack_from("<I", data, 12)[0])
    starts = sorted({struct.unpack_from("<I", data, off)[0] for off in range(12, label_end, 4)})
    label_of = {}
    for i, off in enumerate(range(12, label_end, 4)):
        label_of.setdefault(struct.unpack_from("<I", data, off)[0], i)
    for li, start in enumerate(starts):
        end = starts[li + 1] if li + 1 < len(starts) else str_off
        p = start
        while start <= p < end and p < len(data):
            proc.stdin.write(f"{path} {p:x}\n"); proc.stdin.flush()
            _, n, iname = proc.stdout.readline().strip().split(" ", 2)
            n = int(n)
            if iname == "__Unrecognized__" or n <= 0:
                break
            if data[p] != 0xFE and (data[p] & 0x7F, data[p + 1]) in wanted:
                print(f"{name:16s} {p:06x} L{label_of[start]:<4d} {iname:24s} {data[p:p+n].hex()}")
            p += n
