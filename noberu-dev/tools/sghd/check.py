#!/usr/bin/env python3
"""Compare impacto's VM trace against SGHD instruction lengths.

  check.py TRACE.log SCRIPTS_DIR
SCRIPTS_DIR holds the .SCX files from script.mpk (mpkls.py -x ...). For every pair
of consecutive instructions on one thread in one buffer, where the first isn't
control flow, impacto's step (next - this) should equal the SGHD length. Prints
each opcode whose step disagrees, with one example address.
"""
import os, re, subprocess, sys
from collections import OrderedDict

trace, scripts = sys.argv[1], sys.argv[2]
tool = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sghdlen")
FLOW = re.compile(r"Jump|Call|Return|Loop|EndOfScript|Halt|Terminate|Reset|Switch|Select", re.I)

buffers, thread, last, pairs = {}, None, {}, OrderedDict()
for line in open(trace, errors="replace"):
    m = re.search(r'Loading script "([^"]+)" \(id \d+\) into buffer (\d+)', line)
    if m:
        buffers[int(m.group(2))] = os.path.join(scripts, m.group(1)); continue
    m = re.search(r"Running thread ID = (\d+)", line)
    if m:
        thread = int(m.group(1)); continue
    m = re.search(r"Address: (0x[0-9a-f]+) Opcode: ([0-9a-f:]+) ScriptBuffer: (\d+)", line)
    if not m:
        continue
    addr, op, buf = int(m.group(1), 16), m.group(2), int(m.group(3))
    prev = last.get(thread)
    if prev and prev[2] == buf and buf in buffers and addr != prev[0]:
        pairs.setdefault((buffers[buf], prev[0]), (prev[1], addr - prev[0]))
    last[thread] = (addr, op, buf)

if not pairs:
    sys.exit("no instruction pairs (is Trace logging on, VM channel enabled?)")
proc = subprocess.Popen([tool], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
data = {}
def decode(f, a):
    proc.stdin.write(f"{f} {a:x}\n"); proc.stdin.flush()
    _, length, name = proc.stdout.readline().strip().split(" ", 2)
    return int(length), name

bad = OrderedDict()
for (f, a), (op, step) in pairs.items():
    if step <= 0:
        continue  # thread attribution is lost across nested CreateThread runs
    length, name = decode(f, a)
    if FLOW.search(name):
        continue
    # impacto doesn't log 0xFE (Assign) instructions; walk over them.
    if f not in data:
        data[f] = open(f, "rb").read()
    pos, expect = a + length, {a + length}
    while pos < len(data[f]) and data[f][pos] == 0xFE and len(expect) < 16:
        pos += decode(f, pos)[0]
        expect.add(pos)
    if a + step in expect:
        continue
    bad.setdefault((op, name), []).append(
        f"{os.path.basename(f)}@{a:x} impacto={step} sghd={length}")
print(f"{len(pairs)} steps checked, {len(bad)} opcodes disagree")
for (op, name), ex in bad.items():
    print(f"  {op} {name:28s} x{len(ex):<4d} e.g. {ex[0]}")
