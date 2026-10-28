#include "devinput.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <SDL3/SDL_stdinc.h>

#include "inputsystem.h"
#include "log.h"
#include "vm/interface/input.h"
#include "mem.h"
#include "audio/audiosystem.h"
#include "data/savesystem.h"
#include "games/sghd/phone.h"
#include "games/sghd/titlemenu.h"
#include "profile/charset.h"
#include "profile/scriptvars.h"
#include "profile/vm.h"
#include "profile/game.h"
#include "renderer/renderer.h"
#include "renderer/window.h"
#include "text/dialoguepage.h"
#include "ui/backlogmenu.h"
#include "ui/sysmesbox.h"
#include "ui/ui.h"
#include "io/vfs.h"
#include "video/videosystem.h"
#include "vm/vm.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <utf8.h>

namespace Impacto {
namespace DevInput {

using namespace Impacto::Vm::Interface;

static uint32_t PadBit(const std::string& name) {
  static const std::pair<const char*, uint32_t> names[] = {
      {"A", PAD1A},       {"B", PAD1B},         {"X", PAD1X},
      {"Y", PAD1Y},       {"UP", PAD1UP},       {"DOWN", PAD1DOWN},
      {"LEFT", PAD1LEFT}, {"RIGHT", PAD1RIGHT}, {"START", PAD1START},
      {"SELECT", PAD1SELECT}, {"L1", PAD1L1},   {"R1", PAD1R1},
      {"L2", PAD1L2},     {"R2", PAD1R2},           {"L3", PAD1L3},
      {"R3", PAD1R3}};
  for (const auto& [n, bit] : names)
    if (name == n) return bit;
  return 0;
}

using namespace Impacto::Profile::ScriptVars;

static uint64_t Frame = 0;

// Replies ("DevState ...") go to IMPACTO_DEV_INPUT.out so tests needn't
// search the (trace-level) log.
static void Reply(const std::string& line) {
  static const char* path = SDL_getenv("IMPACTO_DEV_INPUT");
  ImpLog(LogLevel::Info, LogChannel::General, "{}\n", line);
  if (!path) return;
  std::ofstream out(std::string(path) + ".out", std::ios::app);
  out << line << '\n';
}

// Profiles without a CharacterToSc3 table (Steam STEINS;GATE) get the Latin
// rows of their FONT.PNG: 128-137 digits, 138-163 A-Z, 164-189 a-z, and
// punctuation after them.
static uint32_t SghdGlyphChar(uint32_t id) {
  if (id == 0 || id == 63) return ' ';
  if (id >= 128 && id <= 137) return '0' + (id - 128);
  if (id >= 138 && id <= 163) return 'A' + (id - 138);
  if (id >= 164 && id <= 189) return 'a' + (id - 164);
  switch (id) {  // read off the prologue's first lines
    case 192: return ',';
    case 193: return '.';
    case 196: return '?';
    case 197: return '!';
    case 66: return '-';
    case 201: return '\'';
    case 202: return 0x201C;  // left double quote
    case 204: return '(';
    case 414: return 0xA3;  // pound sign
    case 415: return '%';
    case 416: return '#';
    case 417: return '&';
    case 418: return '*';
    case 205: return ')';
    case 203: return 0x201D;  // right double quote
  }
  return 0;
}

static std::string GlyphText(const std::vector<ProcessedTextGlyph>& glyphs,
                             bool markRows = false) {
  static ankerl::unordered_dense::map<uint16_t, uint32_t> sc3ToChar;
  if (sc3ToChar.empty()) {
    for (const auto& [ch, sc3] : Profile::Charset::CharacterToSc3)
      sc3ToChar.emplace(sc3 & 0x7FFF, ch);
  }
  std::string text;
  float rowY = glyphs.empty() ? 0.0f : glyphs.front().DestRect.Y;
  for (const ProcessedTextGlyph& glyph : glyphs) {
    if (markRows && glyph.DestRect.Y != rowY) {
      text += '/';
      rowY = glyph.DestRect.Y;
    }
    const auto found = sc3ToChar.find((uint16_t)glyph.CharId);
    uint32_t ch = found != sc3ToChar.end() ? found->second
                  : sc3ToChar.empty()      ? SghdGlyphChar(glyph.CharId)
                                           : 0;
    if (ch == 0) {
      text += fmt::format("{{{}}}", glyph.CharId);  // unknown glyph: {id}
      continue;
    }
    if (ch == '"' || ch == '\\' || ch < 0x20) ch = ' ';
    utf8::append(ch, std::back_inserter(text));
  }
  return text;
}

static size_t TextRows(const std::vector<ProcessedTextGlyph>& glyphs) {
  std::set<int> rows;
  for (const ProcessedTextGlyph& glyph : glyphs)
    rows.insert((int)glyph.DestRect.Y);
  return rows.size();
}

static int MenuStateOf(const UI::Menu* menu) {
  return menu ? (int)menu->State : -1;
}

// One line of machine-readable state for tests (noberu-dev/tests).
static void ReplyState() {
  const DialoguePage* page = DialoguePages.empty() ? nullptr : &DialoguePages[0];
  int sysmesbox = 0;
  for (const auto& [id, box] : UI::SysMesBox::MsgBoxes)
    if (box->State != UI::Hidden) sysmesbox++;
  const bool sghd =
      Profile::Vm::GameInstructionSet == Vm::InstructionSet::SGHD;
  const auto* title = dynamic_cast<const UI::SGHD::TitleMenu*>(UI::TitleMenuPtr);
  Reply(fmt::format(
      "DevState frame={} gamestate={} titlemode={} script={} line={} "
      "skip={} allskip={} auto={} select={} uihidden={} nolog={} resuming={} "
      "sysmenu={} sysmenucur={} savemenu={} titlemenu={} titlestage={} "
      "titlecur={} titlesub={} backlog={} "
      "sysmesbox={} "
      "phoneopen={} phonect={} phonemode={} glyphs={} rows={} opaque={} "
      "mastervol={} "
      "movie0={} movie1={} movieno={} "
      "name=\"{}\" text=\"{}\" wrapped=\"{}\"",
      Frame, ScrWork[SW_GAMESTATE], (int)GetFlag(SF_TITLEMODE),
      ScrWork[SW_SCRIPTID], ScrWork[SW_LINEID], (int)GetFlag(SF_MESSKIP),
      (int)GetFlag(SF_MESALLSKIP), (int)AutoModeEnabled,
      (int)GetFlag(SF_SELECTMODE), (int)GetFlag(SF_UIHIDDEN),
      (int)GetFlag(SF_BACKLOG_NOLOG), (int)SaveSystem::ResumeLine.has_value(),
      MenuStateOf(UI::SystemMenuPtr),
      ScrWork[SW_SYSMENUCNO],
      MenuStateOf(UI::SaveMenuPtr), MenuStateOf(UI::TitleMenuPtr),
      title ? title->DevStage() : -1, title ? title->DevMainCursor() : -1,
      title ? title->DevSubCursor() : -1, MenuStateOf(UI::BacklogMenuPtr),
      sysmesbox,
      sghd ? (int)GetFlag(2600) : -1,  // phone.cpp SF_PhoneOpen
      sghd ? ScrWork[6801] : -1, sghd ? ScrWork[6802] : -1,
      page ? page->Glyphs.size() : 0, page ? TextRows(page->Glyphs) : 0,
      page ? (int)page->TextIsFullyOpaque() : 0,
      (int)(Audio::MasterVolume * 100),
      Video::Players[0] ? (int)Video::Players[0]->IsPlaying : -1,
      Video::Players[1] ? (int)Video::Players[1]->IsPlaying : -1,
      ScrWork[SW_MOVIE_PLAYNO], page ? GlyphText(page->Name) : "",
      page ? GlyphText(page->Glyphs) : "",
      page ? GlyphText(page->Glyphs, true) : ""));
}

// Device id stamped on events pushed here, so DropEvent can tell them apart.
constexpr Uint32 DevDeviceId = 0x7E57;

bool DropEvent(const SDL_Event* ev) {
  // IMPACTO_DEV_INPUT_ONLY=1 (tests): ignore the real mouse, keyboard, pad
  // and touch, so a pointer resting over the window can't move cursors.
  static const bool only = [] {
    const char* v = SDL_getenv("IMPACTO_DEV_INPUT_ONLY");
    return v && *v == '1';
  }();
  if (!only) return false;
  switch (ev->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      return ev->key.which != DevDeviceId;
    case SDL_EVENT_MOUSE_MOTION:
      return ev->motion.which != DevDeviceId;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      return ev->button.which != DevDeviceId;
    case SDL_EVENT_MOUSE_WHEEL:
      return ev->wheel.which != DevDeviceId;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      return true;
    default:
      return false;
  }
}

static void PushKey(std::string name, bool down) {
  std::ranges::replace(name, '_', ' ');
  const SDL_Scancode code = SDL_GetScancodeFromName(name.c_str());
  if (code == SDL_SCANCODE_UNKNOWN) {
    ImpLog(LogLevel::Warning, LogChannel::General,
           "DevInput: unknown key \"{}\"\n", name);
    return;
  }
  SDL_Event ev{};
  ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  ev.key.scancode = code;
  ev.key.key = SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false);
  ev.key.down = down;
  ev.key.which = DevDeviceId;
  SDL_PushEvent(&ev);
}

// SDL mouse events at design coordinates (the inverse of inputsystem.cpp's
// SDLMouseCoordsToDesign), so clicks take the same path as a real mouse.
static void PushMouse(Uint32 type, float x, float y, Uint8 button) {
  const RectF viewport = Window->GetLogicalViewport();
  SDL_Event ev{};
  ev.type = type;
  const float wx = viewport.X + x * viewport.Width / Profile::Game::DesignWidth;
  const float wy = viewport.Y + y * viewport.Height / Profile::Game::DesignHeight;
  if (type == SDL_EVENT_MOUSE_MOTION) {
    ev.motion.which = DevDeviceId;
    ev.motion.x = wx;
    ev.motion.y = wy;
  } else {
    ev.button.which = DevDeviceId;
    ev.button.x = wx;
    ev.button.y = wy;
    ev.button.button = button;
    ev.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    ev.button.clicks = 1;
  }
  SDL_PushEvent(&ev);
}

static std::string PendingShot;

void AfterRender() {
  if (PendingShot.empty()) return;
  Renderer->CaptureFrame(PendingShot);
  Reply(fmt::format("DevShot {}", PendingShot));
  PendingShot.clear();
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
  Frame++;

  // Read whole lines appended since the last frame. The file is never
  // deleted here: deleting after reading lost commands appended in between.
  static std::streamoff offset = -1;
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    offset = 0;
    return;
  }
  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (offset < 0) offset = size;  // skip commands left from an earlier run
  if (size < offset) offset = 0;  // file was replaced
  if (size == offset) return;
  std::string chunk((size_t)(size - offset), '\0');
  file.seekg(offset);
  file.read(chunk.data(), (std::streamsize)chunk.size());
  const size_t end = chunk.rfind('\n');
  if (end == std::string::npos) return;  // partial line, wait for the rest
  chunk.resize(end + 1);
  offset += (std::streamoff)chunk.size();
  std::stringstream contents(chunk);

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
  Frame++;
    } else if (cmd == "sw" || cmd == "flag") {
      // Dump: "sw 6800 6830" / "flag 2600 2620" (inclusive ranges)
      int from = 0, to = -1;
      in >> from >> to;
      if (to < from) to = from;
      std::string out;
      for (int i = from; i <= to; i++)
        out += fmt::format(" {}={}", i, cmd == "sw" ? ScrWork[i] : (int)GetFlag(i));
      Reply(fmt::format("Dev{}:{}", cmd, out));
    } else if (cmd == "setsw" || cmd == "setflag") {
      int idx = 0, value = 0;
      in >> idx >> value;
      if (cmd == "setsw")
        ScrWork[idx] = value;
      else
        SetFlag(idx, value);
    } else if (cmd == "phonemail") {
      int slot = 0, mail = 0;
      in >> slot >> mail;
      UI::SGHD::Phone::DevReceiveMail(slot, mail);
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
    } else if (cmd == "playmovie") {
      // Plays movie ID (the "movie" mount) on channel 0, like PlayMovie.
      int id = 0;
      in >> id;
      Io::Stream* stream = nullptr;
      if (Io::VfsOpen("movie", (uint32_t)id, &stream) == IoError_OK) {
        if (Video::Players[0]->IsPlaying) Video::Players[0]->Stop();
        Video::Players[0]->Play(stream, false, false);
        Reply(fmt::format("DevMovie {} opened", id));
      } else {
        Reply(fmt::format("DevMovie {} missing", id));
      }
    } else if (cmd == "stopmovie") {
      Video::Players[0]->Stop();
    } else if (cmd == "state") {
      ReplyState();
    } else if (cmd == "key" || cmd == "keydown" || cmd == "keyup") {
      // SDL key events, so the profile's keyboard map is exercised. "key"
      // presses and releases in one frame like a fast tap.
      std::string name;
      in >> name;
      if (cmd != "keyup") PushKey(name, true);
      if (cmd != "keydown") PushKey(name, false);
    } else if (cmd == "mouse" && (in >> x >> y)) {
      PushMouse(SDL_EVENT_MOUSE_MOTION, x, y, 0);
    } else if ((cmd == "mclick" || cmd == "mrclick" || cmd == "mmclick") &&
               (in >> x >> y)) {
      const Uint8 button = cmd == "mclick"    ? SDL_BUTTON_LEFT
                           : cmd == "mrclick" ? SDL_BUTTON_RIGHT
                                              : SDL_BUTTON_MIDDLE;
      PushMouse(SDL_EVENT_MOUSE_MOTION, x, y, 0);
      PushMouse(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y, button);
      PushMouse(SDL_EVENT_MOUSE_BUTTON_UP, x, y, button);
    } else if (cmd == "wheel") {
      float dy = 0;
      in >> dy;
      SDL_Event ev{};
      ev.type = SDL_EVENT_MOUSE_WHEEL;
      ev.wheel.which = DevDeviceId;
      ev.wheel.y = dy;
      ev.wheel.integer_y = (Sint32)dy;
      SDL_PushEvent(&ev);
    } else if (cmd == "shot") {
      in >> PendingShot;
    } else if (cmd == "echo") {
      std::string rest;
      std::getline(in, rest);
      Reply("DevEcho" + rest);
    } else if (cmd == "quit") {
      SDL_Event ev{};
      ev.type = SDL_EVENT_QUIT;
      SDL_PushEvent(&ev);
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
