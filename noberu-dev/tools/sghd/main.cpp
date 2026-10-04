// sghdlen: decode Steam STEINS;GATE (SGHD) SC3 instructions using CoZ's
// sc3ntist operand layouts. Reads "<scx path> <hex address>" lines on stdin,
// prints "<hex address> <length> <name>" per line.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include "SGHDDisassembler.h"

int main() {
  std::map<std::string, std::vector<uint8_t>> files;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string path, addrStr;
    if (!(in >> path >> addrStr)) continue;
    auto& buf = files[path];
    if (buf.empty()) {
      std::ifstream f(path, std::ios::binary);
      buf.assign(std::istreambuf_iterator<char>(f), {});
      buf.resize(buf.size() + 64, 0);
    }
    SCXOffset addr = (SCXOffset)std::stoul(addrStr, nullptr, 16);
    SCXFile file{buf.data()};
    SGHDDisassembler dis(file);
    SC3Instruction* inst = dis.DisassembleAt(addr, (SCXOffset)(buf.size() - addr));
    std::printf("%x %u %s\n", addr, inst->len, inst->name.c_str());
    std::fflush(stdout);
    delete inst;
  }
}
