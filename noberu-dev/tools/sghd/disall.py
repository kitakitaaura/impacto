#!/usr/bin/env python3
"""Disassemble every label of one SCX (label by label, data shown raw).

  disall.py FILE.SCX > out.txt
"""
import os, struct, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
path = sys.argv[1]
data = open(path, "rb").read()
str_off = struct.unpack_from("<I", data, 4)[0]
# The label table runs until the first label's code (not to the string table).
label_end = min(str_off, struct.unpack_from("<I", data, 12)[0])
labels = [struct.unpack_from("<I", data, o)[0] for o in range(12, label_end, 4)]
starts = sorted(set(labels))
names = {}
for i, a in enumerate(labels):
    names.setdefault(a, []).append(f"L{i}")
for i, a in enumerate(starts):
    end = starts[i + 1] if i + 1 < len(starts) else str_off
    print(f"\n== {' '.join(names[a])} @ {a:06x}")
    out = subprocess.run([sys.executable, os.path.join(here, "dis.py"), path, f"{a:x}", "4000"],
                         capture_output=True, text=True).stdout.splitlines()
    for line in out:
        addr = int(line[:6], 16)
        if addr >= end: break
        print(line.split("  <-")[0][:170])
        if "__Unrecognized__" in line: break
