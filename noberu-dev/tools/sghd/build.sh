#!/bin/sh
# Fetch CoZ sc3ntist's SGHD (Steam S;G) decoder - unlicensed upstream, so it is
# downloaded at build time and never committed - and build it against our shims.
set -e
cd "$(dirname "$0")"
mkdir -p upstream
for f in SGHDDisassembler.cpp SGHDDisassembler.h DisassemblerMacros.h; do
  [ -f upstream/$f ] || curl -fsSL "https://raw.githubusercontent.com/CommitteeOfZero/sc3ntist/master/src/parser/$f" -o upstream/$f
done
c++ -std=c++17 -O1 -w -Ishim -Iupstream main.cpp upstream/SGHDDisassembler.cpp -o sghdlen
echo built $(pwd)/sghdlen
