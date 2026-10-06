#include "devinput.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <SDL3/SDL_stdinc.h>

#include "inputsystem.h"
#include "log.h"
#include "vm/interface/input.h"

namespace Impacto {
namespace DevInput {

using namespace Impacto::Vm::Interface;

static uint32_t PadBit(const std::string& name) {
  static const std::pair<const char*, uint32_t> names[] = {
      {"A", PAD1A},       {"B", PAD1B},         {"X", PAD1X},
      {"Y", PAD1Y},       {"UP", PAD1UP},       {"DOWN", PAD1DOWN},
      {"LEFT", PAD1LEFT}, {"RIGHT", PAD1RIGHT}, {"START", PAD1START},
      {"SELECT", PAD1SELECT}, {"L1", PAD1L1},   {"R1", PAD1R1},
      {"L2", PAD1L2},     {"R2", PAD1R2}};
  for (const auto& [n, bit] : names)
    if (name == n) return bit;
  return 0;
}

static void PointerAt(float x, float y) {
  Input::PrevMousePos = Input::CurMousePos;
  Input::CurMousePos = {x, y};
  Input::InitMousePos = Input::CurMousePos;
  Input::CurrentInputDevice = Input::Device::Mouse;
}

void Apply() {
  static const char* path = SDL_getenv("IMPACTO_DEV_INPUT");
  if (!path) return;
  // Buttons held across frames by "hold NAME" until "release NAME".
  static uint32_t held = 0;
  PADinputButtonIsDown |= held;

  std::ifstream file(path);
  if (!file) return;
  std::stringstream contents;
  contents << file.rdbuf();
  file.close();
  std::remove(path);

  std::string line;
  while (std::getline(contents, line)) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    float x = 0, y = 0;
    if (cmd == "pad") {
      std::string name;
      in >> name;
      const uint32_t bit = PadBit(name);
      PADinputButtonWentDown |= bit;
      PADinputButtonIsDown |= bit;
      // Like a real key press: widgets ignore pad confirm while the pointer
      // is the active device.
      Input::CurrentInputDevice = Input::Device::Keyboard;
    } else if (cmd == "hold" || cmd == "release") {
      std::string name;
      in >> name;
      const uint32_t bit = PadBit(name);
      held = cmd == "hold" ? held | bit : held & ~bit;
      PADinputButtonIsDown |= held;
    } else if (cmd == "move" && (in >> x >> y)) {
      PointerAt(x, y);
    } else if (cmd == "click" && (in >> x >> y)) {
      PointerAt(x, y);
      PADinputMouseWentDown |= PAD1A;
      PADinputMouseIsDown |= PAD1A;
    } else if (cmd == "rclick" && (in >> x >> y)) {
      PointerAt(x, y);
      PADinputMouseWentDown |= PAD1B;
      PADinputMouseIsDown |= PAD1B;
    } else if (!cmd.empty()) {
      ImpLog(LogLevel::Warning, LogChannel::General,
             "DevInput: unknown command \"{}\"\n", line);
      continue;
    }
    ImpLog(LogLevel::Debug, LogChannel::General, "DevInput: {}\n", line);
  }
}

}  // namespace DevInput
}  // namespace Impacto
