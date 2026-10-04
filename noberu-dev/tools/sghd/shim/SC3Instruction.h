#pragma once
#include <string>
#include <vector>
#include "SC3Argument.h"
struct SC3Instruction {
  SC3Instruction(SCXOffset pos, SCXOffset len, const std::string& name,
                 const std::vector<SC3Argument>& args)
      : pos(pos), len(len), name(name), args(args) {}
  SCXOffset pos, len;
  std::string name;
  std::vector<SC3Argument> args;
};
