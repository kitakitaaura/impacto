#include "phone.h"

#include <algorithm>
#include <array>
#include <vector>
#include <ctime>

#include "../../vm/vm.h"
#include "../../vm/thread.h"
#include "../../vm/expression.h"
#include "../../vm/interface/input.h"
#include "../../mem.h"
#include "../../log.h"
#include "../../renderer/renderer.h"
#include "../../profile/sprites.h"
#include "../../profile/dialogue.h"
#include "../../text/text.h"
#include "../../audio/audiosystem.h"
#include "../../audio/audiochannel.h"

namespace Impacto {
namespace UI {
namespace SGHD {
namespace Phone {

using namespace Impacto::Vm;
using namespace Impacto::Vm::Interface;

#include "../../vm/inst_macros.inc"

// --- ScrWork / flag ids (Game.exe offsets / 4) ---------------------------
enum : int {
  SW_PhoneCt = 6801,        // open/close counter 0..20
  SW_PhoneMode = 6802,      // current screen
  SW_MailFolder = 6803,     // 0 inbox, 1 sent (0xFF: pointer mode)
  SW_MailPages = 6804,
  SW_MailPage = 6805,
  SW_MailCursor = 6806,
  SW_MailSelected = 6807,   // mail record id
  SW_SentCount = 6808,
  SW_InboxCount = 6809,
  SW_HomeContact = 6811,
  SW_MenuCursor = 6812,
  SW_MailLines = 6814,
  SW_MailNotify = 6815,
  SW_ContactCount = 6820,
  SW_ContactCursor = 6821,
  SW_ContactSelected = 6822,
  SW_RecvSlot = 6823,
  SW_RecvMail = 6824,
  SW_RecvResult = 6825,     // [slot]
  SW_CallTargets = 6830,    // 4 contact ids packed into bytes
  SW_MailPopupCursor = 6847,
  SW_CallTargets2 = 6848,
  SW_CallPopupCursor = 6849,
  SW_PhoneHide1 = 6851,
  SW_PhoneHide2 = 6852,
  SW_PhoneColor = 6856,     // body texture variant 0..4
  SW_PhonePri = 6842,       // render layer
  SW_Wallpaper = 3003,
  SW_Clock = 2015,
};
enum : int {
  SF_PhoneOpen = 2600,
  SF_HasSignal = 2601,
  SF_CallModeIn = 2602,
  SF_CallModeOut = 2603,
  SF_NewMail = 2604,
  SF_Calling = 2605,
  SF_PhoneClose = 2609,
  SF_HomeConfirm = 2607,
  SF_HomeCancel = 2608,
  SF_CallConfirm = 2611,
  SF_Receiving = 2613,
  SF_AllMailRead = 2619,
  SF_CallEvent = 2620,
  SF_MailOpened = 1603,
  SF_NoBackFromBook = 1631,
};

constexpr int MaxMails = 700;
constexpr int MaxContacts = 12;
constexpr int ContactFlagBase = MaxMails * 5;  // 3500
constexpr int None = 0x3E7;                    // 999

// --- Persistent phone data (0x19002c0 bits, 0x19004b8 status bytes) ------
std::array<uint8_t, 500> Bits{};
std::array<uint8_t, MaxMails * 5> MailStatus{};

static bool GetBit(int bit) { return Bits[bit >> 3] & (1 << (bit & 7)); }
static void SetBit(int bit, bool value) {
  if (value)
    Bits[bit >> 3] |= (uint8_t)(1 << (bit & 7));
  else
    Bits[bit >> 3] &= (uint8_t)~(1 << (bit & 7));
}

// --- Tables registered by 10:37:04 from _MAIL.SCX -------------------------
struct MailRecord {
  uint32_t Flags;     // +00 bit0: sent mail
  uint32_t Date;      // +04 MMDD
  uint32_t Time;      // +08 HHMM
  uint32_t F0C;       // +0C
  uint32_t Strings;   // +10 first of the mail's strings (subject, ..., body)
  uint32_t Reply;     // +14
  uint32_t F18, F1C, F20, F24, F28, F2C, F30;
};
std::vector<MailRecord> Mails;
uint32_t TableBuffer = 0;     // [0x1901274]
uint32_t UiStrBase = 0;       // [0x1901278]
uint32_t ContactStrBase = 0;  // [0x190127c]

// Lists rebuilt by 10:37:05 (0x4547f0)
std::vector<uint32_t> InboxList;     // 0x190aa18
std::vector<uint32_t> SentList;      // 0x190a0b8
std::vector<uint32_t> ContactList;   // 0x190b9b8
std::array<std::array<bool, 2>, MaxContacts> ContactCaps{};  // 0x190b9f0
// Address book order (0x5319d8): lower sorts first.
constexpr std::array<int, MaxContacts> ContactOrder = {8, 3, 9, 4,  2, 1,
                                                       10, 0, 5, 6, 11, 7};

void Reset() {
  Bits.fill(0);
  MailStatus.fill(0);
  InboxList.clear();
  SentList.clear();
  ContactList.clear();
  ScrWork[6815] = 0;
}

void GetSaveBits(uint8_t* out) { std::copy(Bits.begin(), Bits.end(), out); }

void SetSaveBits(const uint8_t* in) {
  std::copy(in, in + Bits.size(), Bits.begin());
  for (size_t i = 0; i < MailStatus.size(); i++) MailStatus[i] = GetBit((int)i);
}

static uint32_t Str(uint32_t num) {
  return ScriptGetStrAddress(TableBuffer, num);
}

// --- 10:37 sub-ops ----------------------------------------------------------
static void SetMailStatus(int mail, int cat, int value) {
  const int i = mail * 5 + cat;
  if (i < 0 || i >= (int)MailStatus.size()) return;
  MailStatus[i] = (uint8_t)value;
  SetBit(i, value != 0);
}
static int GetMailStatus(int mail, int cat) {
  const int i = mail * 5 + cat;
  if (i < 0 || i >= (int)MailStatus.size()) return 0;
  return MailStatus[i];
}

static uint16_t ReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static void RegisterTables(uint32_t buffer, const std::array<uint32_t, 6>& labels) {
  TableBuffer = buffer;
  const uint8_t* base = ScriptBuffers[buffer].data();
  UiStrBase = ReadU16(base + labels[1]);
  ContactStrBase = ReadU16(base + labels[2]);

  // Mail table (0x45c040): u16 flags, 10 x u16, then u16 a, u16 strings, 2 x
  // u16 unused; 0xFF ends the table.
  Mails.clear();
  const uint8_t* p = base + labels[0];
  uint16_t flags = ReadU16(p);
  p += 2;
  while (flags != 0xFF && Mails.size() < MaxMails) {
    MailRecord r{};
    r.Flags = flags;
    r.Date = ReadU16(p + 0);
    r.Time = ReadU16(p + 2);
    r.F0C = ReadU16(p + 4);
    r.Reply = ReadU16(p + 6);
    r.F18 = ReadU16(p + 8);
    r.F1C = ReadU16(p + 10);
    r.F20 = ReadU16(p + 12);
    r.F24 = ReadU16(p + 14);
    r.F28 = ReadU16(p + 16);
    r.F2C = ReadU16(p + 18);
    p += 20;
    r.F30 = ReadU16(p + 0);
    r.Strings = ReadU16(p + 2);
    p += 8;
    Mails.push_back(r);
    flags = ReadU16(p);
    p += 2;
  }
  ImpLog(LogLevel::Info, LogChannel::VM,
         "Phone: {} mails, UI strings at {}, contacts at {}\n", Mails.size(),
         UiStrBase, ContactStrBase);
}

static void SortByDate(std::vector<uint32_t>& list) {
  std::stable_sort(list.begin(), list.end(), [](uint32_t a, uint32_t b) {
    const MailRecord& ma = Mails[a];
    const MailRecord& mb = Mails[b];
    if (ma.Date != mb.Date) return ma.Date > mb.Date;
    return ma.Time > mb.Time;
  });
}

// 0x4547f0
static void BuildLists() {
  ScrWork[SW_Wallpaper] = ScrWork[2010];
  InboxList.clear();
  SentList.clear();
  bool allRead = true;
  for (uint32_t i = 0; i < Mails.size(); i++) {
    if (!GetMailStatus(i, 0)) continue;
    if (Mails[i].Flags & 1) {
      SentList.push_back(i);
    } else {
      if (!GetMailStatus(i, 1)) allRead = false;
      InboxList.push_back(i);
    }
  }
  SetFlag(SF_AllMailRead, allRead);
  SortByDate(InboxList);
  SortByDate(SentList);
  ScrWork[SW_SentCount] = (int)SentList.size();
  ScrWork[SW_InboxCount] = (int)InboxList.size();

  ContactList.clear();
  for (int i = 0; i < MaxContacts; i++) {
    if (GetBit(ContactFlagBase + i)) ContactList.push_back(i);
  }
  std::stable_sort(ContactList.begin(), ContactList.end(),
                   [](uint32_t a, uint32_t b) {
                     return ContactOrder[a] < ContactOrder[b];
                   });
  ScrWork[SW_ContactCount] = (int)ContactList.size();

  for (auto& caps : ContactCaps) caps = {false, false};
  for (int b = 0; b < 4; b++) {
    const int out = (ScrWork[SW_CallTargets] >> (8 * b)) & 0xFF;
    const int in = (ScrWork[SW_CallTargets2] >> (8 * b)) & 0xFF;
    if (out < MaxContacts) ContactCaps[out][0] = true;
    if (in < MaxContacts) ContactCaps[in][1] = true;
  }
}

// 0x454730
static void SelectMail() {
  const bool sent = ScrWork[SW_MailFolder] != 0;
  const auto& list = sent ? SentList : InboxList;
  ScrWork[SW_MailPages] = (int)(list.size() / 12);
  const size_t i =
      (size_t)ScrWork[SW_MailPage] * 12 + (size_t)ScrWork[SW_MailCursor];
  ScrWork[SW_MailSelected] = i < list.size() ? (int)list[i] : None;
}

// 0x45ab40
static void ReceiveMail(int slot, int mail) {
  ScrWork[6860] = None;
  ScrWork[6861] = None;
  if (mail == None) return;
  ScrWork[SW_RecvSlot] = slot;
  ScrWork[SW_RecvMail] = mail;
  SetFlag(SF_Receiving, 1);
  if (mail >= 0 && mail < MaxMails) {
    MailStatus[mail * 5] = 1;
    SetBit(mail * 5, true);
  }
  SetFlag(SF_NewMail, 1);
}

// 0x45abd0
static void FinishReceive() {
  if (!GetFlag(SF_Receiving)) return;
  const int slot = ScrWork[SW_RecvSlot];
  const int reply = ScrWork[6861];
  if (slot >= 0 && slot < 8) {
    ScrWork[SW_RecvResult + slot] =
        reply != None && reply >= 0 && reply < (int)Mails.size()
            ? (int)Mails[reply].Reply
            : None;
  }
  ScrWork[6860] = None;
  ScrWork[6861] = None;
  ScrWork[SW_RecvSlot] = 0xFF;
  ScrWork[SW_RecvMail] = None;
  SetFlag(SF_Receiving, 0);
}

// 0x459ff0
static void SetCallTargets(bool incoming, int a, int b, int c, int d) {
  const int packed = (((((d << 8) + c) << 8) + b) << 8) + a;
  ScrWork[incoming ? SW_CallTargets2 : SW_CallTargets] = packed;
}

static void Update();

void Instruction(Sc3VmThread* thread) {
  StartInstruction;
  PopUint8(type);
  switch (type) {
    case 0x00:
    case 0x01: {
      PopUint8(category);
      PopExpression(mail);
      SetMailStatus(mail, category, type == 0);
    } break;
    case 0x02:
    case 0x03: {
      PopUint8(category);
      PopExpression(mail);
      PopLocalLabel(target);
      const bool set = GetMailStatus(mail, category) == 1;
      if (set == (type == 0x02)) thread->IpOffset = target;
    } break;
    case 0x04: {
      std::array<uint32_t, 6> labels{};
      for (auto& label : labels) {
        PopUint16(labelNum);
        label = ScriptGetLabelAddress(thread->ScriptBufferId, labelNum);
      }
      RegisterTables(thread->ScriptBufferId, labels);
    } break;
    case 0x05:
      BuildLists();
      break;
    case 0x06:
      SelectMail();
      break;
    case 0x07:
      Update();
      break;
    case 0x08:  // 0x45d400: no-op in the Steam build
      break;
    case 0x09:  // mail body line counts for scrolling (0x45bc30)
      ScrWork[SW_MailLines] = 8;
      break;
    case 0x0A:  // same for the received mail (0x45bb90)
      ScrWork[6818] = 0;
      ScrWork[6819] = 8;
      break;
    case 0x0F:
    case 0x10: {
      PopExpression(contact);
      if (contact >= 0 && contact < MaxContacts)
        SetBit(ContactFlagBase + contact, type == 0x0F);
    } break;
    case 0x11:
      for (int i = 0; i < MaxContacts; i++) SetBit(ContactFlagBase + i, false);
      break;
    case 0x12: {
      PopExpression(slot);
      PopExpression(mail);
      ReceiveMail(slot, mail);
    } break;
    case 0x13:
      FinishReceive();
      break;
    case 0x14:
    case 0x15: {
      PopExpression(a);
      PopExpression(b);
      PopExpression(c);
      PopExpression(d);
      SetFlag(type == 0x14 ? SF_CallModeOut : SF_CallModeIn, 1);
      SetFlag(SF_CallEvent, 1);
      SetCallTargets(type == 0x15, a, b, c, d);
    } break;
    case 0x16: {
      const int i = ScrWork[SW_ContactCursor];
      ScrWork[SW_ContactSelected] =
          i >= 0 && i < (int)ContactList.size() ? (int)ContactList[i] : 0xFF;
    } break;
    case 0x17:  // number dialling keypad (0x45a350), not ported yet
      SetFlag(SF_PhoneClose, 1);
      break;
    case 0x18:
      break;
    case 0x19: {  // real-time clock into SW 2015 (0x4547b0)
      std::time_t now = std::time(nullptr);
      std::tm* t = std::localtime(&now);
      ScrWork[SW_Clock] =
          (((t->tm_mon + 1) * 100 + t->tm_mday) * 100 + t->tm_hour) * 100 +
          t->tm_min;
    } break;
    case 0x1A: {
      PopExpression(mail);
      if (mail >= 0 && mail < (int)Mails.size()) {
        Mails[mail].Date = ScrWork[SW_Clock] / 10000;
        Mails[mail].Time = ScrWork[SW_Clock] % 10000;
      }
    } break;
    case 0x1E:  // CoZ patch extension, nothing to do here
      break;
    default:
      ImpLog(LogLevel::Warning, LogChannel::VMStub,
             "Phone: unhandled 10:37 sub-op {:#x}\n", type);
      break;
  }
}

// --- Input -----------------------------------------------------------------
static bool Pressed(int slot) {
  return (PADinputButtonWentDown | PADinputMouseWentDown) & PADcustom[slot];
}
static bool Repeated(int slot) {
  return PADinputButtonRepeatDown & PADcustom[slot];
}

static void PlaySE(int id) {
  (void)id;  // TODO: phone sound effects (0x26eee98 request block)
}

// --- Per-screen update handlers (0x45a030) ----------------------------------
// 0x454be0: home screen
static void UpdateHome() {
  if (Pressed(6)) {
    SetFlag(SF_HomeCancel, 1);
    SetFlag(SF_PhoneClose, 1);
    return;
  }
  if (!Pressed(5)) return;
  if (GetFlag(SF_Calling)) {
    SetFlag(SF_HomeConfirm, 1);
    SetFlag(2611, 1);
    SetFlag(SF_PhoneClose, 1);
    return;
  }
  if (GetFlag(SF_NewMail)) {
    SetFlag(SF_NewMail, 0);
    SetFlag(SF_MailOpened, 1);
    PlaySE(0x9c);
    ScrWork[SW_PhoneMode] = 3;
    ScrWork[SW_MailPage] = 0;
    ScrWork[SW_MailCursor] = 0;
    ScrWork[SW_MailFolder] = 0;
    return;
  }
  ScrWork[SW_PhoneMode] = 20;
  ScrWork[SW_MenuCursor] = 0;
  if (GetFlag(SF_CallEvent) && GetFlag(800) &&
      (GetFlag(SF_CallModeOut) || GetFlag(SF_CallModeIn))) {
    ScrWork[SW_MenuCursor] = 1;
  }
  ScrWork[SW_MailNotify] = 0;
}

// Main menu (LanguageBarrier's reimplementation of the same handler).
static void UpdateMenu() {
  int& cursor = ScrWork[SW_MenuCursor];
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 0;
    cursor = 0xFF;
    return;
  }
  if (Pressed(5)) {
    switch (cursor) {
      case 0:
        ScrWork[SW_MailFolder] = 0;
        ScrWork[SW_PhoneMode] = 1;
        break;
      case 1:
        ScrWork[SW_ContactCursor] = 0;
        ScrWork[SW_PhoneMode] = 6;
        break;
      case 3:
        ScrWork[6843] = 0;
        ScrWork[SW_PhoneMode] = 13;
        break;
      default:  // 2: "connect to the network", nothing in the original
        break;
    }
    return;
  }
  if (Pressed(2) || Pressed(3)) cursor = cursor == 0xFF ? 0 : cursor ^ 1;
  if (Pressed(0) || Pressed(1)) cursor = cursor == 0xFF ? 0 : cursor ^ 2;
}

// 0x459870: address book
static void UpdateAddressBook() {
  int& cursor = ScrWork[SW_ContactCursor];
  const int count = (int)ContactList.size();
  if (Pressed(6)) {
    if (!GetFlag(SF_NoBackFromBook)) {
      ScrWork[SW_MenuCursor] = 1;
      ScrWork[SW_PhoneMode] = 20;
    }
    return;
  }
  if (Pressed(5) && cursor != 0xFF && cursor < count) {
    ScrWork[SW_CallPopupCursor] = 0;
    if (GetFlag(SF_HasSignal)) {
      const int contact = (int)ContactList[cursor];
      if ((GetFlag(SF_CallModeIn) && ContactCaps[contact][1]) ||
          (GetFlag(SF_CallModeOut) && ContactCaps[contact][0])) {
        ScrWork[SW_PhoneMode] = 7;
        return;
      }
    }
  }
  if (count == 0) return;
  if (Repeated(0)) cursor = cursor == 0xFF ? 0 : (cursor == 0 ? count : cursor) - 1;
  if (Repeated(1)) cursor = cursor == 0xFF || cursor >= count - 1 ? 0 : cursor + 1;
}

// What the selected contact allows for the armed call/mail event.
static void ContactActions(bool& canIn, bool& canOut) {
  const int cursor = ScrWork[SW_ContactCursor];
  canIn = canOut = false;
  if (cursor < 0 || cursor >= (int)ContactList.size()) return;
  const int contact = (int)ContactList[cursor];
  canIn = GetFlag(SF_CallModeIn) && ContactCaps[contact][1];
  canOut = GetFlag(SF_CallModeOut) && ContactCaps[contact][0];
}

// 0x459ab0: contact picked, Call/Mail popup
static void UpdateContactPopup() {
  bool canIn, canOut;
  ContactActions(canIn, canOut);
  int& popup = ScrWork[SW_CallPopupCursor];
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 6;
    return;
  }
  if (Pressed(5) && popup != 0xFF) {
    ScrWork[SW_ContactSelected] = (int)ContactList[ScrWork[SW_ContactCursor]];
    SetFlag(popup == 0 && canOut ? 2614 : 2615, 1);  // call / mail chosen
    SetFlag(2611, 1);                                 // finish the waiting line
    SetFlag(SF_PhoneClose, 1);
    return;
  }
  if (Repeated(0) || Repeated(1)) {
    if (popup == 0xFF)
      popup = 0;
    else if (canIn && canOut)
      popup ^= 1;
  }
}

static void Update() {
  switch (ScrWork[SW_PhoneMode]) {
    case 7:
      UpdateContactPopup();
      break;
    case 0:
      UpdateHome();
      break;
    case 6:
      UpdateAddressBook();
      break;
    case 20:
      UpdateMenu();
      break;
    default:
      break;  // script-driven screens, or not ported yet
  }
}

void Tick() {
  int& ct = ScrWork[SW_PhoneCt];
  if (GetFlag(SF_PhoneOpen)) {
    if (ct < 20) ct++;
  } else if (ct > 0) {
    ct--;
  }
}

// --- Drawing ----------------------------------------------------------------
// The executable draws in 1920x1080 pixels; impacto's design space is 1280x720.
// Texture rectangles below are Steam texture pixels (PHONE*.DDS, 1.5x the PS3
// art) and positions are design units, exactly as the executable passes them
// before its own *1.5.
static float Alpha = 1.0f;

static const SpriteSheet* BodySheet() {
  static const char* names[] = {"Phone", "PhoneB", "PhoneC", "PhoneD", "PhoneE"};
  const int color = std::clamp(ScrWork[SW_PhoneColor], 0, 4);
  auto it = Profile::SpriteSheets.find(names[color]);
  if (it == Profile::SpriteSheets.end()) it = Profile::SpriteSheets.find("Phone");
  return it == Profile::SpriteSheets.end() ? nullptr : &it->second;
}

static void Spr(float sx, float sy, float sw, float sh, float dx, float dy,
                float alpha = Alpha) {
  const SpriteSheet* sheet = BodySheet();
  if (!sheet) return;
  Sprite s(*sheet, sx / 1.5f, sy / 1.5f, sw / 1.5f, sh / 1.5f);
  Renderer->DrawSprite(s, RectF(dx, dy, sw / 1.5f, sh / 1.5f),
                       glm::vec4(1.0f, 1.0f, 1.0f, alpha));
}

static glm::vec4 Rgb(uint32_t c, float alpha) {
  return glm::vec4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f,
                   (c & 0xFF) / 255.0f, alpha);
}

// 0x42f890, size in texture pixels like the executable
static void Rect(float dx, float dy, float w, float h, uint32_t color) {
  Renderer->DrawQuad(RectF(dx, dy, w / 1.5f, h / 1.5f), Rgb(color, Alpha));
}

// 0x4451e0 for one line (lineCount 1/2 with short strings).
static float Text(float x, float y, uint32_t strNum, uint32_t color,
                  float size) {
  if (!TableBuffer) return 0.0f;
  Sc3VmThread dummy;
  dummy.ScriptBufferId = TableBuffer;
  dummy.IpOffset = Str(strNum);
  std::vector<ProcessedTextGlyph> glyphs = TextLayoutPlainLine(
      &dummy, 255, *Profile::Dialogue::DialogueFont, size,
      DialogueColorPair{color, 0}, 1.0f, glm::vec2(x, y), TextAlignment::Left);
  Profile::Dialogue::DialogueFont->DrawProcessedText(glyphs, Alpha);
  return GetTextWidth(glyphs);
}

// 0x45bf20: screen header bar + title
struct HeaderStyle {
  uint8_t Str, Style;
};
constexpr HeaderStyle Headers[] = {{0, 0},  {1, 0}, {40, 0}, {39, 0},
                                   {41, 2}, {4, 1}, {5, 1},  {6, 1},
                                   {7, 1},  {8, 1}, {44, 0}, {0, 0}};
static void Header(int type, float x, float y) {
  const HeaderStyle h = Headers[type];
  Spr(2656.5f, (h.Style * 31 + 0x41) * 1.5f, 414.0f, 43.5f, x, y);
  Text(x + 32, y + 2, UiStrBase + h.Str, 0xFFFFFF, 24);
}

// 0x45a970: two-option popup
static void Popup(int strA, int strB, int cursor, float x, float y) {
  Spr(2239.5f, 252.0f, 414.0f, 210.0f, x, y + 0x54);
  const float x2 = x + 0x1d;
  const float y2 = y + 0x5c;
  if (cursor != 0xFF) Spr(2434.5f, 1012.5f, 21.0f, 36.0f, x2, y2 + cursor * 24 - 1);
  Text(x2 + 0x14, y2, UiStrBase + strA, cursor == 0 ? 0xFF1111 : 0, 20);
  if (strB != 0x270F)
    Text(x2 + 0x14, y2 + 24, UiStrBase + strB, cursor == 1 ? 0xFF1111 : 0, 20);
}

// 0x44dc00 (idle part): standby screen
static void DrawHome(float x, float y) {
  if (GetFlag(SF_NewMail)) {
    Spr(2239.5f, 1.5f, 414.0f, 247.5f, x, y + 0x4c);
    Text(x + 0x50, y + 0x96, UiStrBase + 9, 0xFF1111, 24);
  }
}

// 0x44fe90: main menu (4 icons)
static void DrawMenu(float x, float y) {
  Spr(2656.5f, 285.0f, 414.0f, 475.5f, x, y);
  const int cursor = ScrWork[SW_MenuCursor];
  const float srcX[] = {2656.5f, 2758.5f, 2860.5f, 2962.5f};
  const float dx[] = {x + 0x2e, x + 0x96, x + 0x2e, x + 0x96};
  const float dy[] = {y + 0x2f, y + 0x2f, y + 0x85, y + 0x85};
  for (int i = 0; i < 4; i++) {
    const float sy = ((cursor == i ? 0x3b3 : 0x36f) + 1) * 1.5f;
    Spr(srcX[i], sy, 99.0f, 99.0f, dx[i], dy[i]);
  }
  if (cursor >= 0 && cursor < 4)
    Text(x + 0x12, y + 0xe0, UiStrBase + 0x23 + cursor, 0xFFFFFF, 20);
}

// 0x453690: address book
static void DrawAddressBook(float x, float y) {
  Header(4, x, y);
  const float y0 = y + 0x1d;
  Rect(x, y0, 414.0f, 424.5f, 0xFFFFFF);
  const int cursor = ScrWork[SW_ContactCursor];
  if (cursor != 0xFF) Rect(x, y0 + cursor * 24, 414.0f, 36.0f, 0xFFBA95);
  for (size_t i = 0; i < ContactList.size(); i++) {
    Text(x + 12, y0 + i * 24, ContactStrBase + ContactList[i] * 2 + 1, 0, 24);
  }
  Spr(2239.5f, 465.0f, 414.0f, 432.0f, x, y0);
}

static void DrawScreen(int mode, float x, float y) {
  switch (mode) {
    case 0:
      DrawHome(x, y);
      break;
    case 6:
      DrawAddressBook(x, y);
      break;
    case 7: {  // 0x453840
      DrawAddressBook(x, y);
      bool canIn, canOut;
      ContactActions(canIn, canOut);
      const int popup = ScrWork[SW_CallPopupCursor];
      if (canIn && canOut)
        Popup(0x10, 0x11, popup, x, y + 0x1d);
      else if (canIn)
        Popup(0x11, 0x270F, popup, x, y + 0x1d);
      else
        Popup(0x10, 0x270F, popup, x, y + 0x1d);
    } break;
    case 20:
    case 21:
      DrawMenu(x, y);
      break;
    case 34:  // mail compose + Send/Save (compose screen not ported yet)
      Rect(x, y + 0x1d, 414.0f, 424.5f, 0xFFFFFF);
      Popup(0x36, 0x37, ScrWork[SW_MailPopupCursor], x, y + 0x1d);
      break;
    case 39: {  // address book + call popup (0x452160)
      DrawAddressBook(x, y);
      const bool in = GetFlag(SF_CallModeIn);
      const bool out = GetFlag(SF_CallModeOut);
      if (in && out)
        Popup(0x10, 0x11, ScrWork[SW_CallPopupCursor], x, y + 0x1d);
      else if (in)
        Popup(0x11, 0x270F, 0, x, y + 0x1d);
      else
        Popup(0x10, 0x270F, 0, x, y + 0x1d);
    } break;
    default:  // 99: on a call (no screen content); others not ported yet
      break;
  }
}

// 0x453b10
static void Render() {
  const int ct = ScrWork[SW_PhoneCt];
  if (ct == 0) return;
  const int left = 0x47a - 14 * ct;
  Alpha = std::min(ct * 256 / 20, 256) / 256.0f;
  // Handset
  Spr(1.5f, 1.5f, 567.0f, 1077.0f, (float)(left - 10), 2.0f);
  const float x = (float)(left + 0x29);
  DrawScreen(ScrWork[SW_PhoneMode], x, 0x91);
  if (ScrWork[SW_PhoneMode] != 0x63) {
    // Status bar (0x44d830): signal / out of range
    Spr(2656.5f, GetFlag(SF_HasSignal) ? 1.5f : 49.5f, 414.0f, 45.0f, x, 0x73);
  }
  // Screen glass
  Spr(616.5f, 631.5f, 414.0f, 447.0f, x, 0x73);
}

void RenderLayer(uint32_t layer) {
  if ((int)layer != ScrWork[SW_PhonePri]) return;
  if (ScrWork[SW_PhoneHide1] >= 0xFF || ScrWork[SW_PhoneHide2] >= 0xFF) return;
  Render();
}

}  // namespace Phone
}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
