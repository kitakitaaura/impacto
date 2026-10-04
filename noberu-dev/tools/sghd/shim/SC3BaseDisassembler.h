#pragma once
#include "SC3Instruction.h"
struct SCXFile {
  uint8_t* data;
  uint8_t* getPData() { return data; }
};
class SC3BaseDisassembler {
 public:
  SC3BaseDisassembler(SCXFile& f) : _file(f) {}
  virtual ~SC3BaseDisassembler() = default;
  virtual SC3Instruction* DisassembleAt(SCXOffset address, SCXOffset maxLength) = 0;
 protected:
  SCXFile& _file;
};
