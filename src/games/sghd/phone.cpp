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
#include "../../profile/scriptvars.h"
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
  SW_WallpaperSet = 2010,
  SW_Ringtone = 2011,
  SW_MailTone = 2013,
  SW_SettingsCursor = 6843,  // 0 wallpaper, 1 ringtone, 2 mail tone
  SW_ToneCursor = 6844,      // row 0 is "none"
  SW_ToneRows = 6845,
  SW_SetPopup = 6846,        // 0 preview/play, 1 set
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

// Reply mail for a body link: the record's eight reply slots (link 0 is Reply).
static int ReplyFor(int mail, int link) {
  if (mail < 0 || mail >= (int)Mails.size() || link < 0 || link > 7) return None;
  const MailRecord& m = Mails[mail];
  const uint32_t slots[8] = {m.Reply, m.F18, m.F1C, m.F20, m.F24, m.F28, m.F2C, m.F30};
  return (int)slots[link];
}

// Screen to return to after sending (0x190ba60): 5 = reply from the viewer,
// 27 = reply chosen by link.
static int ReturnMode = 5;
static int SelectedLink = 0xFF;  // 0x190b9ec
static int SendTimer = 0;        // 0x190b37c
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

// Ringtones (0x190b380, label 4): Track is the BGM track and the SW 2011
// value, MailTone the SW 2013 value, Name a UI string offset, Flag unlocks it.
struct SoundRecord {
  uint32_t Track, MailTone, Name, Flag;
};
std::vector<SoundRecord> Sounds;
// Wallpapers (0x190b5e0, label 5): the record index is the wallpaper id.
struct WallpaperRecord {
  uint32_t Id, Name, Flag;
};
std::vector<WallpaperRecord> Wallpapers;
// Unlocked entries, rebuilt with the other lists (0x45a2b0)
// Mail attachments (0x190ba80, label 3), indexed by a mail's F0C:
// 0/4 picture/movie (Value = movie), 1 picture slideshow, 2 ringtones,
// 3 documents, 5 wallpaper. Data comes from the label numbered Value.
struct AttachmentRecord {
  uint32_t Type, Value;
  std::array<uint32_t, 15> Data;  // up to 5 document triples
};
std::vector<AttachmentRecord> Attachments;
// Documents (10:38:00 from _ATCH.SCX): a u16 label per document, each label
// a 0xFFFF-terminated list of line strings.
static uint32_t DocBuffer = 0, DocTable = 0;  // [0x18ffba8], [0x18ffbac]
std::vector<uint32_t> SoundList;      // 0x190b568
std::vector<uint32_t> WallpaperList;  // 0x190b8b8

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

  // Attachments (0x45c210): u16 type (0xFF ends), u16 value, then data read
  // from the label numbered value.
  Attachments.clear();
  p = base + labels[3];
  for (uint16_t type = ReadU16(p); type != 0xFF; type = ReadU16(p)) {
    AttachmentRecord r{};
    r.Type = type;
    r.Value = ReadU16(p + 2);
    p += 4;
    r.Data.fill(0xFF);
    if (type == 1 || type == 2 || type == 3 || type == 5) {
      const uint8_t* d = base + ScriptGetLabelAddress(buffer, r.Value);
      if (type == 3) {
        for (int i = 0; i < 5; i++, d += 6) {  // (document, lines, flag)
          r.Data[3 * i] = ReadU16(d);
          if (r.Data[3 * i] == 0xFF) break;
          r.Data[3 * i + 1] = ReadU16(d + 2);
          r.Data[3 * i + 2] = ReadU16(d + 4);
        }
      } else {
        const int max = type == 2 ? 5 : 10;
        for (int i = 0; i < max; i++, d += 2) {
          r.Data[i] = ReadU16(d);
          if (r.Data[i] == 0xFF) break;
        }
      }
    }
    Attachments.push_back(r);
  }

  // Sound and wallpaper tables: u16 fields, 0xFFFF ends.
  Sounds.clear();
  for (p = base + labels[4]; ReadU16(p) != 0xFFFF; p += 8)
    Sounds.push_back({ReadU16(p), ReadU16(p + 2), ReadU16(p + 4), ReadU16(p + 6)});
  Wallpapers.clear();
  for (p = base + labels[5]; ReadU16(p) != 0xFFFF; p += 6)
    Wallpapers.push_back({ReadU16(p), ReadU16(p + 2), ReadU16(p + 4)});

  ImpLog(LogLevel::Info, LogChannel::VM,
         "Phone: {} mails, {} attachments, {} sounds, {} wallpapers, UI "
         "strings at {}, contacts at {}\n",
         Mails.size(), Attachments.size(), Sounds.size(), Wallpapers.size(),
         UiStrBase,
         ContactStrBase);
}

static void SortByDate(std::vector<uint32_t>& list) {
  std::stable_sort(list.begin(), list.end(), [](uint32_t a, uint32_t b) {
    const MailRecord& ma = Mails[a];
    const MailRecord& mb = Mails[b];
    if (ma.Date != mb.Date) return ma.Date > mb.Date;
    return ma.Time > mb.Time;
  });
}

static void BuildUnlockLists();

// 0x4547f0
static void BuildLists() {
  ScrWork[SW_Wallpaper] = ScrWork[SW_WallpaperSet];
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
  BuildUnlockLists();
}

// 0x45a2b0: ringtones and wallpapers whose flag is 0 or set
static void BuildUnlockLists() {
  SoundList.clear();
  for (uint32_t i = 0; i < Sounds.size(); i++)
    if (!Sounds[i].Flag || GetFlag(Sounds[i].Flag)) SoundList.push_back(i);
  WallpaperList.clear();
  for (uint32_t i = 0; i < Wallpapers.size(); i++)
    if (!Wallpapers[i].Flag || GetFlag(Wallpapers[i].Flag)) WallpaperList.push_back(i);
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
static void UpdateKeypad();
static void OpenAttachment(int mail, bool fromLink);
static void ReceiveMail(int slot, int mail);
void DevReceiveMail(int slot, int mail) { ReceiveMail(slot, mail); }
static int LineCount(uint32_t strNum, float width, float size);
static std::vector<int> VisibleLinks(int mail, int scroll);
struct LinkBox {
  int Link;
  RectF Box;
};
static std::vector<LinkBox> LinkBoxes(int mail, int scroll, float x, float y0);

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
    case 0x17:
      UpdateKeypad();
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
// Mouse and touch follow the executable's phone hitboxes (group 20 at
// 0x531a68, 1080p pixels relative to the phone screen at 1360,217): hovering
// moves the cursor and a click only confirms on a hitbox. Right click backs
// out like the cancel button.
static bool ClickConfirm = false;  // a hitbox click this frame acts as confirm

static bool Pressed(int slot) {
  if (slot == 5) return (PADinputButtonWentDown & PADcustom[slot]) || ClickConfirm;
  return (PADinputButtonWentDown | PADinputMouseWentDown) & PADcustom[slot];
}

static bool PointerMode() {
  return Input::CurrentInputDevice == Input::Device::Mouse ||
         Input::CurrentInputDevice == Input::Device::Touch;
}
static float ScreenOriginX() {
  return (float)(0x47a - 14 * ScrWork[SW_PhoneCt] + 0x29);
}
// Phone-screen-relative hitbox (1080p px) in design units
static RectF Box(float x, float y, float w, float h) {
  return RectF(ScreenOriginX() + x / 1.5f, 145.0f + y / 1.5f, w / 1.5f, h / 1.5f);
}
static bool Over(const RectF& r) {
  return PointerMode() && r.ContainsPoint(Input::CurMousePos);
}
static bool Moved() { return Input::PrevMousePos != Input::CurMousePos; }
static bool Clicked(const RectF& r) {
  return (PADinputMouseWentDown & PAD1A) && r.ContainsPoint(Input::CurMousePos);
}
static RectF RowBox(int i) { return Box(0, 43 + 36.0f * i, 414, 36); }  // 12..23
static RectF ScreenBox() { return Box(0, 0, 414, 476); }                // 1

// List rows: hover picks the row, a click on a row confirms.
static void PointerList(int& cursor, int count) {
  if (!PointerMode()) return;
  for (int i = 0; i < count && i < 12; i++) {
    if (Over(RowBox(i)) && Moved()) cursor = i;
    if (Clicked(RowBox(i))) {
      cursor = i;
      ClickConfirm = true;
    }
  }
}
// Two-option popups (hitboxes 10, 11)
static void PointerPopup(int& cursor, int options) {
  if (!PointerMode()) return;
  for (int i = 0; i < options; i++) {
    const RectF r = Box(30, 179 + 36.0f * i, 353, 36);
    if (Over(r) && Moved()) cursor = i;
    if (Clicked(r)) {
      cursor = i;
      ClickConfirm = true;
    }
  }
}
// Scrolling text: wheel and the up/down strips (46, 47); returns -1/0/1.
static int PointerScroll() {
  if (!PointerMode()) return 0;
  if (Input::MouseWheelDeltaY > 0 || Clicked(Box(0, 39, 414, 28))) return -1;
  if (Input::MouseWheelDeltaY < 0 || Clicked(Box(0, 452, 414, 28))) return 1;
  return 0;
}
static bool Repeated(int slot) {
  return PADinputButtonRepeatDown & PADcustom[slot];
}

// Phone sound effects: the 0x26eee98 request block is SE channel 2,
// played once (the executable also clears SW_SEREQNO + 2).
static void PlaySE(int id) {
  using namespace Profile::ScriptVars;
  ScrWork[SW_SEREQNO + 2] = 0xFFFF;
  Audio::Channels[Audio::AC_SE2]->SetVolume((ScrWork[SW_SEVOL + 2] / 100.0f) * 0.3f);
  Audio::Channels[Audio::AC_SE2]->Play("se", id, false, 0.0f);
}

// --- Dial keypad (10:37:17, 0x45a350) --------------------------------------
// A 3x4 keypad drawn over the middle of the screen while the phone shows mode
// 23. The script loops 10:37:17 until flag 2609, then branches on SW 6833.
enum {
  SW_KeypadCursor = 6831,  // key 0..11 (0xFF: none under the pointer)
  SW_KeypadCount = 6832,   // digits typed so far
  SW_KeypadResult = 6833,  // 0: #120, 1: 120#, 2: anything else
  SW_KeypadAlpha = 6815,   // overlay fade, set by the script
};
// Key -> digit (0x531a08); 10 and 11 are the # and * keys.
constexpr std::array<int, 12> KeypadDigits = {1, 2, 3, 4, 5, 6,
                                              7, 8, 9, 11, 0, 10};
static std::array<int, 4> KeypadInput{};  // 0x190ba50

// Hitboxes 34..45: absolute 1080p rectangles.
static RectF KeypadKeyBox(int key) {
  static const float xs[] = {775, 900, 1018}, ws[] = {124, 118, 124};
  static const float ys[] = {414, 484, 555, 625};
  return RectF(xs[key % 3] / 1.5f, ys[key / 3] / 1.5f, ws[key % 3] / 1.5f,
               70 / 1.5f);
}

// Number keys type directly (not in the executable).
static int KeypadKeyTyped() {
  using namespace Input;
  const bool shift = KeyboardButtonIsDown[SDL_SCANCODE_LSHIFT] ||
                     KeyboardButtonIsDown[SDL_SCANCODE_RSHIFT];
  if (KeyboardButtonWentDown[SDL_SCANCODE_KP_MULTIPLY] ||
      (shift && KeyboardButtonWentDown[SDL_SCANCODE_8]))
    return 9;  // *
  if (KeyboardButtonWentDown[SDL_SCANCODE_KP_HASH] ||
      (shift && KeyboardButtonWentDown[SDL_SCANCODE_3]))
    return 11;  // #
  if (KeyboardButtonWentDown[SDL_SCANCODE_0] ||
      KeyboardButtonWentDown[SDL_SCANCODE_KP_0])
    return 10;
  for (int d = 1; d <= 9; d++) {
    if ((!shift && KeyboardButtonWentDown[SDL_SCANCODE_1 + d - 1]) ||
        KeyboardButtonWentDown[SDL_SCANCODE_KP_1 + d - 1])
      return d - 1;
  }
  return -1;
}

static void UpdateKeypad() {
  int& cursor = ScrWork[SW_KeypadCursor];
  int& count = ScrWork[SW_KeypadCount];
  ClickConfirm = false;
  if (Pressed(6)) {
    if (count > 0) count--;
    return;
  }
  if (PointerMode()) {
    cursor = 0xFF;
    for (int i = 0; i < 12; i++) {
      if (KeypadKeyBox(i).ContainsPoint(Input::CurMousePos)) cursor = i;
      if (Clicked(KeypadKeyBox(i))) ClickConfirm = true;
    }
  }
  const int typed = KeypadKeyTyped();
  if (typed >= 0) {
    cursor = typed;
    ClickConfirm = true;
  }
  if (Pressed(5)) {
    if (cursor == 0xFF || count >= 4) return;
    PlaySE(0x9c);
    KeypadInput[count++] = KeypadDigits[cursor];
    if (count < 4) return;
    SetFlag(SF_PhoneClose, 1);
    const auto& d = KeypadInput;
    if (d[0] == 10 && d[1] == 1 && d[2] == 2 && d[3] == 0)
      ScrWork[SW_KeypadResult] = 0;
    else if (d[0] == 1 && d[1] == 2 && d[2] == 0 && d[3] == 10)
      ScrWork[SW_KeypadResult] = 1;
    else
      ScrWork[SW_KeypadResult] = 2;
    return;
  }
  // Arrow keys wrap around the 3x4 grid.
  auto move = [&](int slot, auto step) {
    if (!Repeated(slot)) return;
    cursor = cursor == 0xFF ? 0 : step(cursor);
  };
  move(0, [](int c) { return c > 2 ? c - 3 : c + 9; });
  move(1, [](int c) { return c < 9 ? c + 3 : c - 9; });
  move(2, [](int c) { return c % 3 == 0 ? c + 2 : c - 1; });
  move(3, [](int c) { return c % 3 == 2 ? c - 2 : c + 1; });
}

// --- Per-screen update handlers (0x45a030) ----------------------------------
// 0x454be0: home screen
static void UpdateHome() {
  if (Clicked(ScreenBox())) ClickConfirm = true;
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
  if (PointerMode()) {
    // Icons: mail (85,71) / address book (241,71) / network (85,199) /
    // settings (241,199)
    const RectF icons[4] = {Box(85, 71, 88, 88), Box(241, 71, 88, 88),
                            Box(85, 199, 88, 88), Box(241, 199, 88, 88)};
    for (int i = 0; i < 4; i++) {
      if (Over(icons[i]) && Moved()) cursor = i;
      if (Clicked(icons[i])) {
        cursor = i;
        ClickConfirm = true;
      }
    }
  }
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
        ScrWork[SW_SettingsCursor] = PointerMode() ? 0xFF : 0;
        ScrWork[SW_PhoneMode] = 13;
        BuildUnlockLists();
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
  PointerList(cursor, count);
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

static uint32_t MailStr(int mail, int k) {
  return mail >= 0 && mail < (int)Mails.size() ? Mails[mail].Strings + k : 0;
}

// Body + subject line counts for the viewer's scroll range (SW 6814).
static void CountMailLines(int mail) {
  const int body = LineCount(MailStr(mail, 2), 0x116, 24);
  const int subject = std::max(1, LineCount(MailStr(mail, 0), 0xfe, 24));
  ScrWork[SW_MailLines] = body + 2 + subject;
}

static bool HasAttachment(int mail) {
  return mail >= 0 && mail < (int)Mails.size() && Mails[mail].F0C != None;
}

// 0x455df0: Inbox / Outbox
static void UpdateMailFolders() {
  int& cursor = ScrWork[SW_MailFolder];
  for (int i = 0; i < 2 && PointerMode(); i++) {
    const RectF r = Box(0, 90 + 36.0f * i, 414, 36);
    if (Over(r) && Moved()) cursor = i;
    if (Clicked(r)) {
      cursor = i;
      ClickConfirm = true;
    }
  }
  if (Pressed(6)) {
    ScrWork[SW_MenuCursor] = 0;
    ScrWork[SW_PhoneMode] = 20;
    return;
  }
  if (Pressed(5) && cursor != 0xFF) {
    ScrWork[SW_MailPage] = 0;
    ScrWork[SW_MailCursor] = 0;
    ScrWork[SW_PhoneMode] = cursor == 0 ? 3 : 2;
    return;
  }
  if (Repeated(0) || Repeated(1)) cursor = cursor == 0xFF ? 0 : cursor ^ 1;
}

// Shared list paging (0x457fe0 / 0x458b40). Returns true on confirm.
static bool UpdateMailList(const std::vector<uint32_t>& list) {
  const int count = (int)list.size();
  int& page = ScrWork[SW_MailPage];
  int& cursor = ScrWork[SW_MailCursor];
  if (count == 0) {
    if (Pressed(6) || Pressed(5)) ScrWork[SW_PhoneMode] = 1;
    return false;
  }
  const int pages = (count - 1) / 12;
  ScrWork[SW_MailPages] = pages;
  if (PointerMode()) {
    const int onPage = std::min(12, count - page * 12);
    PointerList(cursor, onPage);
    if (Clicked(Box(0, 0, 207, 43)) || Input::MouseWheelDeltaY > 0)
      page = page > 0 ? page - 1 : pages;
    if (Clicked(Box(207, 0, 207, 43)) || Input::MouseWheelDeltaY < 0)
      page = page < pages ? page + 1 : 0;
  }
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 1;
    return false;
  }
  if (Pressed(5) && cursor != 0xFF) return true;
  if (Repeated(0)) {
    if (cursor == 0xFF) cursor = 0;
    else if (cursor > 0) cursor--;
    else if (page > 0) { page--; cursor = 11; }
    else { page = pages; cursor = count - page * 12 - 1; }
  }
  if (Repeated(1) && cursor != 0xFF) {
    if (cursor < 11 && page * 12 + cursor < count - 1) cursor++;
    else { page = page < pages ? page + 1 : 0; cursor = 0; }
  } else if (Repeated(1)) {
    cursor = 0;
  }
  if (Repeated(2)) { cursor = 0; page = page > 0 ? page - 1 : pages; }
  if (Repeated(3)) { cursor = 0; page = page < pages ? page + 1 : 0; }
  const size_t i = (size_t)page * 12 + (size_t)cursor;
  if (cursor != 0xFF && i < list.size()) ScrWork[SW_MailSelected] = (int)list[i];
  return false;
}

static void UpdateSentList() {
  if (!UpdateMailList(SentList)) return;
  ScrWork[6813] = 0;
  CountMailLines(ScrWork[SW_MailSelected]);
  ScrWork[SW_PhoneMode] = 4;
}

static void UpdateInbox() {
  if (!UpdateMailList(InboxList)) return;
  const int mail = ScrWork[SW_MailSelected];
  ScrWork[6813] = 0;
  CountMailLines(mail);
  if (GetFlag(SF_Receiving) && mail == ScrWork[SW_RecvMail] &&
      mail >= 0 && mail < (int)Mails.size() && Mails[mail].Reply != None) {
    SelectedLink = 0xFF;
    ScrWork[SW_PhoneMode] = 27;  // reply to the mail being received
  } else {
    ScrWork[SW_PhoneMode] = 5;
  }
  SetMailStatus(mail, 1, 1);  // read
}

static void ScrollMail() {
  int& scroll = ScrWork[6813];
  const int lines = ScrWork[SW_MailLines];
  const int wheel = PointerScroll();
  if ((Repeated(0) || wheel < 0) && scroll > 0) scroll--;
  if ((Repeated(1) || wheel > 0) && lines > 12 && scroll < lines - 12) scroll++;
}

// Viewers: a click on the page (not on the scroll strips) continues.
static void PointerPageConfirm() {
  if (Clicked(ScreenBox()) && !Clicked(Box(0, 39, 414, 28)) &&
      !Clicked(Box(0, 452, 414, 28)))
    ClickConfirm = true;
}

// 0x4592e0: viewing a sent mail
static void UpdateSentView() {
  const int mail = ScrWork[SW_MailSelected];
  PointerPageConfirm();
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 2;
    return;
  }
  if (Pressed(5)) {
    if (HasAttachment(mail)) {
      ScrWork[6816] = 0;
      ScrWork[SW_PhoneMode] = 19;
    } else {
      ScrWork[SW_PhoneMode] = 2;
    }
    return;
  }
  ScrollMail();
}

// 0x459530: viewing a received mail
static void UpdateInboxView() {
  const int mail = ScrWork[SW_MailSelected];
  PointerPageConfirm();
  const bool scripted =
      GetMailStatus(mail, 3) == 1 && GetMailStatus(mail, 2) == 0;
  if (Pressed(6) || (Pressed(5) && scripted)) {
    if (scripted) {  // the story waits on this mail: hand back to the script
      SetFlag(SF_PhoneClose, 1);
      SetFlag(2606, 1);
      SetFlag(2611, 1);
    } else {
      ScrWork[SW_PhoneMode] = 3;
    }
    return;
  }
  if (Pressed(5)) {
    const bool canReply = GetFlag(SF_HasSignal) && GetMailStatus(mail, 2) == 1 &&
                          GetMailStatus(mail, 4) == 0 && mail >= 0 &&
                          mail < (int)Mails.size() && Mails[mail].Reply != None;
    if (HasAttachment(mail) || canReply) {
      ScrWork[6816] = 0;
      ScrWork[SW_PhoneMode] = 19;
    } else {
      ScrWork[SW_PhoneMode] = 3;
    }
    return;
  }
  ScrollMail();
}

// Viewer reply rule (0x455780): signal, waiting for a reply, not yet replied.
static bool CanReplyFromView(int mail) {
  return GetFlag(SF_HasSignal) && GetMailStatus(mail, 2) == 1 &&
         GetMailStatus(mail, 4) == 0 && mail >= 0 && mail < (int)Mails.size() &&
         Mails[mail].Reply != None;
}

// Link-mail reply rule (0x457620): the mail being received, a link chosen.
static bool CanReplyByLink(int mail) {
  return GetFlag(SF_HasSignal) && GetFlag(SF_Receiving) &&
         mail == ScrWork[SW_RecvMail] && mail >= 0 && mail < (int)Mails.size() &&
         Mails[mail].Reply != None && GetMailStatus(mail, 4) == 0 &&
         SelectedLink != 0xFF;
}

static void SetReplyDraft(int reply) {
  ScrWork[6817] = reply;
  ScrWork[6818] = 0;
  ScrWork[6819] = LineCount(MailStr(reply, 2), 0x116, 24) + 1 +
                  std::max(1, LineCount(MailStr(reply, 0), 0xfe, 24));
}

// Sending a reply (0x456bb0 / 0x459140): mark it sent and the original replied.
static void SendReply() {
  const int reply = ScrWork[6817];
  const int original = ScrWork[SW_MailSelected];
  SetMailStatus(reply, 0, 1);
  if (GetFlag(2610)) SentList.insert(SentList.begin(), (uint32_t)reply);
  if (original != 0xFFFF) SetMailStatus(original, 4, 1);
  SendTimer = 60;
  ScrWork[SW_PhoneMode] = 9;
}

// 0x455780: Reply / Attachments over a mail
static void UpdateMailOptions() {
  const int mail = ScrWork[SW_MailSelected];
  const bool canReply = CanReplyFromView(mail);
  const int options = (canReply ? 1 : 0) + (HasAttachment(mail) ? 1 : 0);
  int& cursor = ScrWork[6816];
  PointerPopup(cursor, options);
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = ScrWork[SW_MailFolder] != 0 ? 4 : 5;
    return;
  }
  if (Pressed(5) && cursor != 0xFF) {
    if (canReply && cursor == 0) {
      ScrWork[SW_PhoneMode] = 22;
      ReturnMode = 5;
      if (GetMailStatus(mail, 3) != 0) {  // the story writes this reply
        SetFlag(SF_PhoneClose, 1);
        SetFlag(2606, 1);
        SetFlag(2611, 1);
      }
      SetReplyDraft(Mails[mail].Reply);
    } else {
      OpenAttachment(mail, false);
    }
    return;
  }
  if (Repeated(0) || Repeated(1)) {
    if (cursor == 0xFF) cursor = 0;
    else if (options > 1) cursor ^= 1;
  }
}

// 0x456f40: received mail with selectable reply links
static void UpdateLinkMail() {
  const int mail = ScrWork[SW_MailSelected];
  int& scroll = ScrWork[6813];
  const int lines = ScrWork[SW_MailLines];
  if (PointerMode()) {
    const int wheel = PointerScroll();
    if (wheel < 0 && scroll > 0) scroll--;
    if (wheel > 0 && lines > 12 && scroll < lines - 12) scroll++;
    for (const LinkBox& b : LinkBoxes(mail, scroll, ScreenOriginX(), 145.0f + 0x1d)) {
      if (Over(b.Box) && Moved()) SelectedLink = b.Link;
      if (Clicked(b.Box)) {
        SelectedLink = b.Link;
        ClickConfirm = true;
      }
    }
  }
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 3;
    return;
  }
  if (Pressed(5)) {
    if (HasAttachment(mail) || CanReplyByLink(mail)) {
      ScrWork[6816] = 0;
      ScrWork[SW_PhoneMode] = 28;
    } else if (!(GetFlag(SF_HasSignal) && GetFlag(SF_Receiving) &&
                 mail == ScrWork[SW_RecvMail])) {
      ScrWork[SW_PhoneMode] = 3;
    }
    return;
  }
  const bool up = Repeated(0), down = Repeated(1);
  if (!up && !down) return;
  std::vector<int> vis = VisibleLinks(mail, scroll);
  const auto at = std::find(vis.begin(), vis.end(), SelectedLink);
  const int cur = at == vis.end() ? -1 : (int)(at - vis.begin());
  bool scrolled = false;
  if (up) {
    if (cur == -1 && !vis.empty()) { SelectedLink = vis.front(); return; }
    if (cur > 0) { SelectedLink = vis[cur - 1]; return; }
    if (scroll > 0) { scroll--; scrolled = true; }
  } else {
    if (cur == -1 && !vis.empty()) { SelectedLink = vis.front(); return; }
    if (cur >= 0 && cur + 1 < (int)vis.size()) { SelectedLink = vis[cur + 1]; return; }
    if (lines > 12 && scroll < lines - 12) { scroll++; scrolled = true; }
  }
  if (!scrolled) return;
  vis = VisibleLinks(mail, scroll);
  if (vis.empty())
    SelectedLink = 0xFF;
  else
    SelectedLink = up ? vis.front() : vis.back();
}

// 0x457620: Reply / Attachments over the link mail
static void UpdateLinkMailOptions() {
  const int mail = ScrWork[SW_MailSelected];
  const bool canReply = CanReplyByLink(mail);
  const int options = (canReply ? 1 : 0) + (HasAttachment(mail) ? 1 : 0);
  int& cursor = ScrWork[6816];
  PointerPopup(cursor, options);
  if (Pressed(6)) {
    SelectedLink = 0xFF;
    ScrWork[SW_PhoneMode] = 27;
    return;
  }
  if (Pressed(5) && cursor != 0xFF) {
    if (canReply && cursor == 0) {
      ScrWork[SW_PhoneMode] = 22;
      ReturnMode = 27;
      const int reply = ReplyFor(mail, SelectedLink);
      ScrWork[6860] = mail;
      ScrWork[6861] = reply;
      SetReplyDraft(reply);
    } else {
      OpenAttachment(mail, true);
    }
    return;
  }
  if (Repeated(0) || Repeated(1)) {
    if (cursor == 0xFF) cursor = 0;
    else if (options > 1) cursor ^= 1;
  }
}

// 0x456bb0: reply preview
static void UpdateReplyPreview() {
  int& scroll = ScrWork[6818];
  const int lines = ScrWork[6819];
  PointerPageConfirm();
  const int wheel = PointerScroll();
  if (wheel < 0 && scroll > 0) scroll--;
  if (wheel > 0 && lines > 12 && scroll < lines - 12) scroll++;
  if (Pressed(6) && ReturnMode == 27) {
    ScrWork[6860] = None;
    ScrWork[6861] = None;
    SelectedLink = 0xFF;
    ScrWork[SW_PhoneMode] = 27;
    return;
  }
  if (Pressed(5)) {
    if (ReturnMode == 27) {
      ScrWork[SW_MailPopupCursor] = 0;
      ScrWork[SW_PhoneMode] = 40;
    } else {
      SendReply();
    }
    return;
  }
  if (Repeated(0) && scroll > 0) scroll--;
  if (Repeated(1) && lines > 12 && scroll < lines - 12) scroll++;
}

// 0x459140: Send?
static void UpdateSendConfirm() {
  PointerPopup(ScrWork[SW_MailPopupCursor], 1);
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 22;
    return;
  }
  if (Pressed(5) && ScrWork[SW_MailPopupCursor] != 0xFF) {
    SendReply();
    return;
  }
  if (Repeated(0) || Repeated(1)) ScrWork[SW_MailPopupCursor] = 0;
}

// 0x459d10: sending (1 s), then "Mail Sent!" (0x456eb0)
static void UpdateSending() {
  if (SendTimer > 0) {
    SendTimer--;
    return;
  }
  PlaySE(0xa2);
  ScrWork[SW_PhoneMode] = 24;
}

static void UpdateMailSent() {
  if (Clicked(ScreenBox())) ClickConfirm = true;
  if ((Pressed(5) || Pressed(6)) && ScrWork[SW_MailSelected] != 0xFFFF)
    ScrWork[SW_PhoneMode] = ReturnMode;
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
  PointerPopup(popup, canIn && canOut ? 2 : 1);
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

// --- Settings: wallpaper, ringtone, mail tone ------------------------------
static int WallCursor = 0xFF;  // 0x190b9a8: row on the page
static int WallPage = 0;       // 0x190b9ac
static int WallPages = 1;      // 0x190b9b0
static int WallHover = 0;      // 0x190c220: page arrow under the pointer
static int PreviewReturn = 15;   // 0x190ba60
static uint64_t PreviewStart = 0; // 0x190ba68 counts 60 Hz frames instead
static int PreviewSound = 0;     // 0x190ba70
static int MarqueeOffset = 0;    // 0x190ba74
static int MarqueeTimer = 0x78;  // 0x190ba78

static bool RingtoneScreen() { return ScrWork[SW_SettingsCursor] == 1; }
static int ToneListMode() { return RingtoneScreen() ? 15 : 16; }
static uint32_t ToneValue(uint32_t sound, bool mail) {
  return mail ? Sounds[sound].MailTone : Sounds[sound].Track;
}

// Two-option popups (hitboxes 10, 11); the exe resets the cursor each frame.
static void PointerPopupReset(int& cursor, bool second) {
  if (!PointerMode()) return;
  cursor = 0xFF;
  for (int i = 0; i < (second ? 2 : 1); i++) {
    const RectF r = Box(30, 179 + 36.0f * i, 353, 36);
    if (Over(r)) cursor = i;
    if (Clicked(r)) ClickConfirm = true;
  }
}
// Click anywhere on the screen (hitbox 0) confirms.
static void PointerAnywhere() {
  if (PointerMode() && (PADinputMouseWentDown & PAD1A)) ClickConfirm = true;
}

// 0x454e30: settings menu
static void UpdateSettings() {
  int& cursor = ScrWork[SW_SettingsCursor];
  if (Pressed(6)) {
    ScrWork[SW_MenuCursor] = 3;
    ScrWork[SW_PhoneMode] = 20;
    return;
  }
  if (PointerMode()) {
    cursor = 0xFF;
    for (int i = 0; i < 3; i++) {
      if (Over(Box(0, 90 + 36.0f * i, 414, 36))) cursor = i;
      if (Clicked(Box(0, 90 + 36.0f * i, 414, 36))) ClickConfirm = true;
    }
  }
  if (Pressed(5)) {
    switch (cursor) {
      case 0: {
        const int count = (int)WallpaperList.size();
        const auto it = std::find(WallpaperList.begin(), WallpaperList.end(),
                                  (uint32_t)ScrWork[SW_WallpaperSet]);
        int idx = it != WallpaperList.end() ? (int)(it - WallpaperList.begin())
                                            : (WallCursor == 0xFF ? 0 : WallCursor);
        WallPages = std::max((count + 5) / 6, 1);
        WallPage = idx / 6;
        WallCursor = PointerMode() ? 0xFF : idx % 6;
        ScrWork[SW_PhoneMode] = 14;
      } break;
      case 1:
      case 2: {
        const bool mail = cursor == 2;
        int& row = ScrWork[SW_ToneCursor];
        row = 0;
        const int value = ScrWork[mail ? SW_MailTone : SW_Ringtone];
        if (value != 0xFFFF) {
          for (size_t i = 0; i < SoundList.size(); i++)
            if ((int)ToneValue(SoundList[i], mail) == value) row = (int)i + 1;
        }
        ScrWork[SW_ToneRows] = (int)SoundList.size() + 1;
        if (PointerMode()) row = 0xFF;
        ScrWork[SW_PhoneMode] = mail ? 16 : 15;
      } break;
      default:
        break;
    }
    return;
  }
  if (Repeated(0)) cursor = cursor == 0xFF ? 0 : (cursor == 0 ? 2 : cursor - 1);
  if (Repeated(1)) cursor = cursor == 0xFF || cursor >= 2 ? 0 : cursor + 1;
}

// 0x4551a0: wallpaper thumbnails, six per page
static void UpdateWallpaperList() {
  const int count = (int)WallpaperList.size();
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 13;
    ScrWork[SW_Wallpaper] = ScrWork[SW_WallpaperSet];
    if (PointerMode()) ScrWork[SW_SettingsCursor] = 0xFF;
    return;
  }
  const auto prevPage = [&] { WallPage = WallPage ? WallPage - 1 : WallPages - 1; };
  const auto nextPage = [&] { WallPage = WallPage < WallPages - 1 ? WallPage + 1 : 0; };
  if (PointerMode()) {
    WallCursor = 0xFF;
    WallHover = 0;
    if (Input::MouseWheelDeltaY > 0) prevPage();
    if (Input::MouseWheelDeltaY < 0) nextPage();
    if (Over(Box(0, 0, 207, 43))) WallHover = 1;
    if (Over(Box(207, 0, 207, 43))) WallHover = 2;
    if (Clicked(Box(0, 0, 207, 43))) prevPage();
    if (Clicked(Box(207, 0, 207, 43))) nextPage();
    for (int i = 0; i < 6; i++) {
      if (WallPage * 6 + i >= count) break;
      const RectF r = Box(0, 43 + 72.0f * i, 414, 72);
      if (Over(r)) WallCursor = i;
      if (Clicked(r)) ClickConfirm = true;
    }
  }
  if (Pressed(5) && WallCursor != 0xFF) {
    ScrWork[SW_SetPopup] = PointerMode() ? 0xFF : 0;
    ScrWork[SW_PhoneMode] = 35;
  }
  if (count == 0) return;
  if (Repeated(0)) {
    if (WallCursor == 0xFF) {
      WallCursor = 0;
    } else if (WallCursor != 0) {
      WallCursor--;
    } else if (WallPage) {
      WallPage--;
      WallCursor = 5;
    } else {
      WallPage = WallPages - 1;
      WallCursor = count - WallPage * 6 - 1;
    }
  }
  if (Repeated(1)) {
    if (WallCursor == 0xFF) {
      WallCursor = 0;
    } else if (WallPage * 6 + WallCursor + 1 == count) {
      WallPage = 0;
      WallCursor = 0;
    } else if (WallCursor < 5) {
      WallCursor++;
    } else {
      WallPage++;
      WallCursor = 0;
    }
  }
  if (Repeated(2)) {
    WallCursor = 0;
    prevPage();
  }
  if (Repeated(3)) {
    WallCursor = 0;
    nextPage();
  }
}

// 0x455510: ringtone / mail tone list
static void UpdateToneList() {
  int& cursor = ScrWork[SW_ToneCursor];
  const int rows = ScrWork[SW_ToneRows];
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 13;
    if (PointerMode()) ScrWork[SW_SettingsCursor] = 0xFF;
    return;
  }
  if (PointerMode()) {
    cursor = 0xFF;
    for (int i = 0; i < 12 && i < rows; i++) {
      if (Over(RowBox(i))) cursor = i;
      if (Clicked(RowBox(i))) ClickConfirm = true;
    }
  }
  if (Pressed(5) && cursor != 0xFF) {
    ScrWork[SW_SetPopup] = PointerMode() ? 0xFF : 0;
    ScrWork[SW_PhoneMode] = 29;
  }
  if (Repeated(0)) cursor = cursor == 0xFF ? 0 : (cursor == 0 ? rows - 1 : cursor - 1);
  if (Repeated(1)) cursor = cursor == 0xFF || cursor >= rows - 1 ? 0 : cursor + 1;
}

static void PlayTone(int track) {
  Audio::Channels[Audio::AC_BGM0]->Play("bgm", track, true, 0.0f);
}

// 0x457cb0: Play / Set popup over the tone list
static void UpdateTonePopup() {
  int& popup = ScrWork[SW_SetPopup];
  const int row = ScrWork[SW_ToneCursor];
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = ToneListMode();
    if (PointerMode()) ScrWork[SW_ToneCursor] = 0xFF;
    return;
  }
  PointerPopupReset(popup, row != 0);
  if (Pressed(5) && popup != 0xFF && row != 0xFF) {
    const bool mail = !RingtoneScreen();
    if (popup == 0 && row != 0) {  // play
      ScrWork[SW_PhoneMode] = 31;
      PreviewReturn = ToneListMode();
      PreviewStart = SDL_GetTicks();
      PreviewSound = (int)SoundList[row - 1];
      MarqueeOffset = 0;
      MarqueeTimer = 0x78;
      PlayTone((int)Sounds[PreviewSound].Track);
    } else {  // set
      ScrWork[SW_PhoneMode] = 30;
      ScrWork[mail ? SW_MailTone : SW_Ringtone] =
          row == 0 ? 0xFFFF : (int)ToneValue(SoundList[row - 1], mail);
    }
    return;
  }
  if (Repeated(0) || Repeated(1)) {
    if (popup == 0xFF)
      popup = 0;
    else if (row != 0)
      popup ^= 1;
  }
}

// 0x4584d0: "ringtone set" notice
static void UpdateToneSet() {
  PointerAnywhere();
  if (Pressed(6) || Pressed(5)) {
    ScrWork[SW_PhoneMode] = ToneListMode();
    if (PointerMode()) ScrWork[SW_ToneCursor] = 0xFF;
  }
}

// 0x458560: music player preview; leaving restores the scene's BGM
static void UpdateTonePreview() {
  PointerAnywhere();
  if (Pressed(6) || Pressed(5)) {
    ScrWork[SW_PhoneMode] = PreviewReturn;
    const int bgm = ScrWork[Profile::ScriptVars::SW_BGMREQNO];
    if (bgm != 0xFFFF)
      Audio::Channels[Audio::AC_BGM0]->Play("bgm", bgm, GetFlag(2484), 0.0f);
    else
      Audio::Channels[Audio::AC_BGM0]->Stop(0.0f);
  }
}

// 0x458810: Preview / Set popup over the wallpaper list
static void UpdateWallpaperPopup() {
  int& popup = ScrWork[SW_SetPopup];
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 14;
    if (PointerMode()) WallCursor = 0xFF;
    return;
  }
  PointerPopupReset(popup, true);
  if (Pressed(5) && popup != 0xFF && WallCursor != 0xFF) {
    const size_t i = (size_t)(WallPage * 6 + WallCursor);
    if (i >= WallpaperList.size()) return;
    const int id = (int)WallpaperList[i];
    ScrWork[SW_Wallpaper] = id;
    if (popup == 1) {
      ScrWork[SW_WallpaperSet] = id;
      ScrWork[SW_PhoneMode] = 36;
    } else {
      ScrWork[SW_PhoneMode] = 37;
    }
    return;
  }
  if (Repeated(0) || Repeated(1)) popup = popup == 0xFF ? 0 : popup ^ 1;
}

// 0x4589d0: "wallpaper set" notice
static void UpdateWallpaperSet() {
  PointerAnywhere();
  if (Pressed(6) || Pressed(5)) {
    ScrWork[SW_PhoneMode] = 14;
    if (PointerMode()) WallCursor = 0xFF;
  }
}

// 0x458a70: full-screen wallpaper preview
static void UpdateWallpaperPreview() {
  PointerAnywhere();
  if (Pressed(6) || Pressed(5)) {
    ScrWork[SW_PhoneMode] = 35;
    if (PointerMode()) ScrWork[SW_SetPopup] = 0xFF;
  }
}

// --- Attachments (modes 200-209) --------------------------------------------
// The handset fades out (SW 6851 / 6852 / 6834 count to 0x100) and the
// attachment is drawn full screen; flags 2502 and 2616 are held off meanwhile.
enum {
  SW_AttachFade = 6851,   // picture / movie
  SW_SlideFade = 6852,    // slideshow, wallpaper
  SW_SlideA = 6853,       // PACG picture shown while flag 1607 is clear
  SW_SlideB = 6855,       // ... while it is set
  SW_DocFade = 6834,
  SW_DocLayer = 6835,
  SW_Doc = 6836,
  SW_DocScroll = 6837,
  SW_DocMaxScroll = 6839,
  SW_AttachMovie = 6850,
  SW_ZoomTimer = 3004,
};
static int AttachIdx = 0;         // 0x190ba7c
static int AttachPos = 0;         // 0x190c210
static bool SlideOther = false;   // 0x190c20c
static bool SavedFlag2502 = false, SavedFlag2616 = false;  // 0x190c204/208
static int DocHover = 0;          // 0x190c224

static const AttachmentRecord* CurrentAttachment() {
  return AttachIdx >= 0 && AttachIdx < (int)Attachments.size()
             ? &Attachments[AttachIdx]
             : nullptr;
}
static uint32_t AttachData(int i) {
  const AttachmentRecord* a = CurrentAttachment();
  return a && i >= 0 && i < (int)a->Data.size() ? a->Data[i] : 0xFF;
}
static void HoldAttachFlags() {
  SavedFlag2502 = GetFlag(2502);
  SavedFlag2616 = GetFlag(2616);
  SetFlag(2616, 0);
  SetFlag(2502, 0);
}
static void RestoreAttachFlags() {
  SetFlag(2616, SavedFlag2616);
  SetFlag(2502, SavedFlag2502);
}
static void UnlockWallpaper(uint32_t wp) {
  if (wp != (uint32_t)None && wp < Wallpapers.size() && Wallpapers[wp].Flag)
    SetFlag(Wallpapers[wp].Flag, 1);
}
static void StartAttachTone(uint32_t sound) {
  if (sound >= Sounds.size()) return;
  PlayTone((int)Sounds[sound].Track);
  if (Sounds[sound].Flag) SetFlag(Sounds[sound].Flag, 1);
  PreviewStart = SDL_GetTicks();
  PreviewSound = (int)sound;
  MarqueeOffset = 0;
  MarqueeTimer = 0x78;
}
static void SetDocPage(int page) {
  ScrWork[SW_Doc] = (int)AttachData(3 * page);
  const int lines = (int)AttachData(3 * page + 1);
  ScrWork[SW_DocMaxScroll] = lines > 0x20 ? lines - 0x20 : 0;
  if (const uint32_t flag = AttachData(3 * page + 2)) SetFlag(flag, 1);
  ScrWork[SW_DocScroll] = 0;
}

// 0x455a02 / 0x457900: "Attachment" chosen from a mail's options popup.
// (Viewing attachments also runs an achievement check, 0x45cb10, not ported.)
static void OpenAttachment(int mail, bool fromLink) {
  AttachIdx = (int)Mails[mail].F0C;
  const AttachmentRecord* a = CurrentAttachment();
  if (!a) return;
  ReturnMode = fromLink ? 27 : 5;
  switch (a->Type) {
    case 0:
    case 4:  // movie in the big frame (Bink, played by _SYSTEM)
      SetFlag(2618, a->Type == 4);
      ScrWork[SW_PhoneMode] = 200;
      ScrWork[6330] = ScrWork[SW_PhonePri];
      HoldAttachFlags();
      break;
    case 2:  // ringtones in the music player
      ScrWork[SW_PhoneMode] = 206;
      AttachPos = 1;
      StartAttachTone(AttachData(0));
      break;
    case 3:  // documents
      ScrWork[SW_PhoneMode] = 204;
      HoldAttachFlags();
      SetFlag(1605, 1);
      ScrWork[SW_DocLayer] = ScrWork[SW_PhonePri];
      AttachPos = 0;
      SetDocPage(AttachPos++);
      break;
    case 1:
    case 5:  // pictures (5: one wallpaper with a zoom effect)
      if (a->Type == 5) {
        ScrWork[SW_PhoneMode] = 207;
        ScrWork[SW_ZoomTimer] = 0;
      } else {
        ScrWork[SW_PhoneMode] = 202;
      }
      if (!fromLink) ReturnMode = ScrWork[SW_MailFolder] ? 4 : 5;
      HoldAttachFlags();
      SetFlag(1605, 1);
      ScrWork[SW_SlideA] = (int)AttachData(0);
      UnlockWallpaper(AttachData(1));
      AttachPos = 2;  // next (picture, wallpaper) pair
      SlideOther = false;
      SetFlag(1607, 0);
      break;
    default:
      break;
  }
}

static bool FadeIn(int sw) {
  if (ScrWork[sw] >= 0x100) return false;
  ScrWork[sw] = std::min(ScrWork[sw] + 0x10, 0x100);
  return true;
}
static bool FadeOut(int sw) {
  if (ScrWork[sw] <= 0) return false;
  ScrWork[sw] = std::max(ScrWork[sw] - 0x10, 0);
  return true;
}

// 0x455f90 / 0x456050: movie attachment
static void UpdateAttachMovie() {
  if (ScrWork[SW_AttachFade] < 0x100) {
    FadeIn(SW_AttachFade);
    if (ScrWork[SW_AttachFade] == 0x100) {
      SetFlag(1602, 1);
      if (const AttachmentRecord* a = CurrentAttachment())
        ScrWork[SW_AttachMovie] = (int)a->Value;
    }
    return;
  }
  PointerAnywhere();
  if ((Pressed(6) || Pressed(5)) && !GetFlag(1612)) ScrWork[SW_PhoneMode] = 201;
}
static void UpdateAttachClose(int fadeSw) {
  RestoreAttachFlags();
  if (!FadeOut(fadeSw)) ScrWork[SW_PhoneMode] = ReturnMode;
}

// 0x4560a0: picture slideshow, confirm shows the next picture
static void UpdateSlides() {
  if (SlideOther) {
    SetFlag(1607, 1);
  } else {
    SetFlag(1607, 0);
    if (FadeIn(SW_SlideFade)) return;
  }
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 203;
    return;
  }
  PointerAnywhere();
  if (!Pressed(5)) return;
  const uint32_t pic = AttachData(AttachPos++);
  if (pic == 0xFF) {
    ScrWork[SW_PhoneMode] = 203;
    return;
  }
  UnlockWallpaper(AttachData(AttachPos++));
  SlideOther = !SlideOther;
  if (SlideOther) {
    SetFlag(1606, 1);
    ScrWork[SW_SlideB] = (int)pic;
  } else {
    SetFlag(1605, 1);
    ScrWork[SW_SlideA] = (int)pic;
  }
}

// 0x456250: document pages with scrolling
static void UpdateDocument() {
  int& scroll = ScrWork[SW_DocScroll];
  const int maxScroll = ScrWork[SW_DocMaxScroll];
  if (FadeIn(SW_DocFade)) return;
  if (Pressed(6)) {
    ScrWork[SW_PhoneMode] = 205;
    return;
  }
  DocHover = 0;
  if (PointerMode()) {
    const RectF up(546 / 1.5f, 66 / 1.5f, 828 / 1.5f, 32 / 1.5f);
    const RectF down(546 / 1.5f, 986 / 1.5f, 828 / 1.5f, 32 / 1.5f);
    if (Input::MouseWheelDeltaY > 0 && scroll > 0) scroll--;
    if (Input::MouseWheelDeltaY < 0 && scroll < maxScroll) scroll++;
    if (Over(up)) DocHover = 1;
    if (Over(down)) DocHover = 2;
    if (Clicked(up)) {
      if (scroll > 0) scroll--;
    } else if (Clicked(down)) {
      if (scroll < maxScroll) scroll++;
    } else {
      PointerAnywhere();
    }
  }
  if (Pressed(5)) {
    if (AttachData(3 * AttachPos) == 0xFF) {
      ScrWork[SW_PhoneMode] = 205;
      return;
    }
    SetDocPage(AttachPos++);
    return;
  }
  if (Repeated(0) && scroll > 0) scroll--;
  if (Repeated(1) && scroll < maxScroll) scroll++;
}

// 0x456530: ringtone attachment in the music player; confirm plays the next
static void UpdateAttachTones() {
  PointerAnywhere();
  const bool back = Pressed(6);
  if (Pressed(5)) {
    const uint32_t next = AttachData(AttachPos++);
    if (next != 0xFF) {
      StartAttachTone(next);
      return;
    }
  } else if (!back) {
    return;
  }
  ScrWork[SW_PhoneMode] = ReturnMode;
  const int bgm = ScrWork[Profile::ScriptVars::SW_BGMREQNO];
  if (bgm != 0xFFFF)
    Audio::Channels[Audio::AC_BGM0]->Play("bgm", bgm, GetFlag(2484), 0.0f);
  else
    Audio::Channels[Audio::AC_BGM0]->Stop(0.0f);
}

// 0x4566e0 / 0x456760 / 0x4567f0: wallpaper attachment. The executable
// zooms the picture with a particle effect (0x45b620) while SW 3004 runs;
// only the timing is kept here.
static void UpdateWallpaperAttach() {
  if (FadeIn(SW_SlideFade)) return;
  PointerAnywhere();
  if (Pressed(6) || Pressed(5)) ScrWork[SW_PhoneMode] = 208;
}
static void UpdateWallpaperZoom() {
  int& t = ScrWork[SW_ZoomTimer];
  if (t < 0x3d) {
    if (++t == 0x3d) SetFlag(3876, 1);
    return;
  }
  PointerAnywhere();
  if (Pressed(6) || Pressed(5)) ScrWork[SW_PhoneMode] = 209;
}
static void UpdateWallpaperAttachClose() {
  int& t = ScrWork[SW_ZoomTimer];
  if (t > 0) {
    t--;
    return;
  }
  UpdateAttachClose(SW_SlideFade);
}

static void Update() {
  ClickConfirm = false;
  switch (ScrWork[SW_PhoneMode]) {
    case 1:
      UpdateMailFolders();
      break;
    case 2:
      UpdateSentList();
      break;
    case 3:
      UpdateInbox();
      break;
    case 4:
      UpdateSentView();
      break;
    case 5:
      UpdateInboxView();
      break;
    case 7:
      UpdateContactPopup();
      break;
    case 9:
      UpdateSending();
      break;
    case 19:
      UpdateMailOptions();
      break;
    case 22:
      UpdateReplyPreview();
      break;
    case 24:
      UpdateMailSent();
      break;
    case 27:
      UpdateLinkMail();
      break;
    case 28:
      UpdateLinkMailOptions();
      break;
    case 40:
      UpdateSendConfirm();
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
    case 13:
      UpdateSettings();
      break;
    case 14:
      UpdateWallpaperList();
      break;
    case 15:
    case 16:
      UpdateToneList();
      break;
    case 29:
      UpdateTonePopup();
      break;
    case 30:
      UpdateToneSet();
      break;
    case 31:
      UpdateTonePreview();
      break;
    case 35:
      UpdateWallpaperPopup();
      break;
    case 36:
      UpdateWallpaperSet();
      break;
    case 37:
      UpdateWallpaperPreview();
      break;
    case 200:
      UpdateAttachMovie();
      break;
    case 201:
      UpdateAttachClose(SW_AttachFade);
      break;
    case 202:
      UpdateSlides();
      break;
    case 203:
      UpdateAttachClose(SW_SlideFade);
      break;
    case 204:
      UpdateDocument();
      break;
    case 205:
      UpdateAttachClose(SW_DocFade);
      break;
    case 206:
      UpdateAttachTones();
      break;
    case 207:
      UpdateWallpaperAttach();
      break;
    case 208:
      UpdateWallpaperZoom();
      break;
    case 209:
      UpdateWallpaperAttachClose();
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

// 0x45c4a0: home wallpaper (texture slot 170 = PWCG.DDS, 9 per row)
static void DrawWallpaper(float x, float y) {
  const int id = ScrWork[SW_Wallpaper];
  if (id == 0xFFFF || id < 0) return;
  const auto it = Profile::SpriteSheets.find("PhoneWallpaper");
  if (it == Profile::SpriteSheets.end()) return;
  const float sx = (id % 9) * 415.0f, sy = (id / 9) * 477.0f;
  Sprite s(it->second, sx / 1.5f, sy / 1.5f, 414.0f / 1.5f, 476.0f / 1.5f);
  Renderer->DrawSprite(s, RectF(x, y, 414.0f / 1.5f, 476.0f / 1.5f),
                       glm::vec4(1.0f, 1.0f, 1.0f, Alpha));
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

// --- Phone text (0x4451e0): word-wrapped SC3 text ------------------------
// Words break at space glyphs (0 / 63), long words split, 0x00 breaks the
// line, 0x09..0x0B mark a mail reply link (as LanguageBarrier's re-
// implementation of the same routine).
struct PhoneGlyph {
  uint16_t Id;
  float Advance;
  DialogueColorPair Colors;
  int Link;  // -1: not a link
};
using PhoneLine = std::vector<PhoneGlyph>;

static std::vector<PhoneLine> WrapText(uint32_t strNum, float width, float size,
                                       uint32_t color) {
  std::vector<PhoneLine> lines(1);
  if (!TableBuffer) return lines;
  const Font& font = *Profile::Dialogue::DialogueFont;
  width -= 4;  // CoZ's phone x padding (2 per side)

  struct Word {
    std::vector<PhoneGlyph> Glyphs;
    float Cost = 0;
    bool LeadingSpace = false;
    bool Break = false;
  };
  std::vector<Word> words(1);
  Sc3VmThread dummy;
  dummy.ScriptBufferId = TableBuffer;
  dummy.IpOffset = Str(strNum);
  DialogueColorPair colors{color, 0};
  int link = -1, links = 0;
  StringToken token;
  for (int guard = 0; guard < 4096; guard++) {
    token.Read(&dummy);
    if (token.Type == STT_EndOfString) break;
    switch (token.Type) {
      case STT_LineBreak:
        words.back().Break = true;
        words.emplace_back();
        break;
      case STT_SetColor:
        if (token.Val_Expr >= 0 && token.Val_Expr < (int)Profile::Dialogue::ColorTable.size())
          colors = Profile::Dialogue::ColorTable[token.Val_Expr];
        break;
      case STT_RubyBaseStart:  // 0x09: link start
        link = links++;
        break;
      case STT_RubyTextEnd:  // 0x0B: link end
        link = -1;
        break;
      case STT_Character: {
        const uint16_t id = token.Val_Int;
        const float adv = font.GetAdvanceWidth(id) * size / font.BitmapEmWidth;
        if (id == 0 || id == 63) {
          words.emplace_back();
          words.back().LeadingSpace = true;
        } else if (words.back().Cost + adv > width && !words.back().Glyphs.empty()) {
          words.emplace_back();
        }
        words.back().Glyphs.push_back({id, adv, colors, link});
        words.back().Cost += adv;
      } break;
      default:
        break;
    }
  }

  float x = 0;
  for (Word& w : words) {
    if (x > 0 && x + w.Cost > width) {
      lines.emplace_back();
      x = 0;
    }
    size_t first = 0;
    if (x == 0 && w.LeadingSpace && !w.Glyphs.empty()) {
      first = 1;  // no leading space at the start of a line
      x -= w.Glyphs[0].Advance;
    }
    for (size_t i = first; i < w.Glyphs.size(); i++) lines.back().push_back(w.Glyphs[i]);
    x += w.Cost;
    if (w.Break) {
      lines.emplace_back();
      x = 0;
    }
  }
  if (lines.size() > 1 && lines.back().empty()) lines.pop_back();
  return lines;
}

// 0x4427f0
static int LineCount(uint32_t strNum, float width, float size) {
  return (int)WrapText(strNum, width, size, 0).size();
}

// Draws lines [skip, skip + count) one glyph-size apart; returns lines drawn.
static int TextLines(float x, float y, float width, uint32_t strNum, int skip,
                     int count, uint32_t color, float size, int selectedLink = -2,
                     uint32_t linkColor = 0, uint32_t selectedColor = 0) {
  const std::vector<PhoneLine> lines = WrapText(strNum, width, size, color);
  const Font& font = *Profile::Dialogue::DialogueFont;
  std::vector<ProcessedTextGlyph> out;
  int drawn = 0;
  for (int i = std::max(skip, 0); i < (int)lines.size() && drawn < count; i++, drawn++) {
    float dx = x + 2;
    const float dy = y + drawn * size;
    for (const PhoneGlyph& g : lines[i]) {
      if (g.Link >= 0 && g.Link == selectedLink && selectedLink >= 0)
        Renderer->DrawQuad(RectF(dx, dy, g.Advance, size), Rgb(0x0000FF, Alpha));
      DialogueColorPair c = g.Colors;
      if (g.Link >= 0 && selectedLink != -2)
        c.TextColor = g.Link == selectedLink ? selectedColor : linkColor;
      out.push_back(font.PlaceGlyph(g.Id, {dx, dy}, size, c, 1.0f));
      dx += g.Advance;
    }
  }
  Profile::Dialogue::DialogueFont->DrawProcessedText(out, Alpha);
  return drawn;
}

// 0x445f10: plain ASCII (dates, page numbers). Glyph ids follow the CoZ
// patch font order (LanguageBarrier's Charset.utf8): ' ', digits, A-Z, a-z,
// then U+3000 and punctuation from id 64.
static void Ascii(float x, float y, const std::string& text, uint32_t color,
                  float size) {
  static constexpr std::string_view order =
      " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  static constexpr std::string_view punct = "/:-;!?'.@#%~*";
  const Font& font = *Profile::Dialogue::DialogueFont;
  std::vector<ProcessedTextGlyph> out;
  float dx = x;
  for (char c : text) {
    uint16_t id = 0;
    if (const size_t i = order.find(c); i != std::string_view::npos)
      id = (uint16_t)i;
    else if (const size_t j = punct.find(c); j != std::string_view::npos)
      id = (uint16_t)(64 + j);
    out.push_back(font.PlaceGlyph(id, {dx, y}, size, DialogueColorPair{color, 0}, 1.0f));
    dx += font.GetAdvanceWidth(id) * size / font.BitmapEmWidth;
  }
  Profile::Dialogue::DialogueFont->DrawProcessedText(out, Alpha);
}

static float MeasureText(uint32_t strNum, float size) {
  if (!TableBuffer) return 0.0f;
  Sc3VmThread dummy;
  dummy.ScriptBufferId = TableBuffer;
  dummy.IpOffset = Str(strNum);
  return GetTextWidth(TextLayoutPlainLine(
      &dummy, 255, *Profile::Dialogue::DialogueFont, size,
      DialogueColorPair{0, 0}, 1.0f, glm::vec2(0.0f), TextAlignment::Left));
}

// Centered on x + 0x8a like the executable (width capped at maxWidth).
static void TextCentered(float x, float y, uint32_t strNum, uint32_t color,
                         float size, float maxWidth) {
  const float w = std::min(MeasureText(strNum, size), maxWidth);
  Text(x + 0x8a - w / 2, y, strNum, color, size);
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

// Raw SC3 glyph ids of a table string (0x80xx pairs, 0xFF ends).
static std::vector<uint16_t> Glyphs(uint32_t strNum) {
  std::vector<uint16_t> out;
  if (!TableBuffer) return out;
  const auto& buf = ScriptBuffers[TableBuffer];
  for (size_t p = Str(strNum); p + 1 < buf.size() && buf[p] != 0xFF;) {
    if (buf[p] & 0x80) {
      out.push_back((uint16_t)(((buf[p] & 0x7F) << 8) | buf[p + 1]));
      p += 2;
    } else {
      p++;  // control byte
    }
  }
  return out;
}

// 0x44e040 helper: the contact's number in big keypad digits. String
// ContactStrBase is the keypad alphabet; each glyph's index picks the digit.
static void DrawNumber(int contact, float x, float y) {
  const std::vector<uint16_t> keys = Glyphs(ContactStrBase);
  const std::vector<uint16_t> number = Glyphs(ContactStrBase + contact * 2 + 2);
  float dx = x;
  for (uint16_t glyph : number) {
    const auto it = std::find(keys.begin(), keys.end(), glyph);
    if (it != keys.end()) {
      const int idx = (int)(it - keys.begin());
      Spr((idx * 24 + 0x14f) * 1.5f, 1366.5f, 33.0f, 45.0f, dx, y);
    }
    dx += 24;
  }
}

// 0x454440 / 0x454170: marching dots around the call display and the music
// player (8 frames, step every 5); top row of n dots, then 5 / n-1 / 4.
static int DotTimer = 0, DotPhase = 0;
static void DrawDots(float x0, float y0, int n) {
  if (++DotTimer > 4) {
    DotTimer = 0;
    DotPhase = (DotPhase + 1) & 7;
  }
  int phase = DotPhase;
  const auto dot = [&](float dx, float dy) {
    Spr((phase * 11 + 0x14f) * 1.5f, 1350.0f, 13.5f, 13.5f, dx, dy);
    phase = (phase + 1) & 7;
  };
  float dx = x0, dy = y0;
  for (int i = 0; i < n; i++, dx += 12) dot(dx, dy);
  dx -= 12;
  for (int i = 0; i < 5; i++) dot(dx, dy += 12);
  for (int i = 0; i < n - 1; i++) dot(dx -= 12, dy);
  for (int i = 0; i < 4; i++) dot(dx, dy -= 12);
}
static void DrawCallDots(float x, float y) { DrawDots(x + 0x42, y + 0x2a, 12); }

// 0x44e040: calling (number, name, "Calling")
static void DrawCalling(float x, float y) {
  Rect(x, y, 414.0f, 468.0f, 0x000000);
  DrawCallDots(x, y);
  const int contact = ScrWork[SW_HomeContact];
  DrawNumber(contact, x + 6, y + 0xd4);
  Text(x + 0x12, y + 0xb0, ContactStrBase + contact * 2 + 1, 0xFFFFFF, 24);
  TextCentered(x, y + 0x40, UiStrBase + 0xf, 0xFFFFFF, 24, 0xc0);
}

static std::string TwoDigits(int v) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "%02d", v % 100);
  return buf;
}

// 0x45a880: message panel (e.g. empty folder)
static void Notice(int str, float x, float y, uint32_t color) {
  Spr(2239.5f, 252.0f, 414.0f, 210.0f, x, y + 0x54);
  TextLines(x + 0x1d, y + 0x64, 0xdc, UiStrBase + str, 0, 4, color, 20);
}

// 0x44f8f0: Inbox / Outbox
static void DrawMailFolders(float x, float y) {
  Spr(2656.5f, 763.5f, 414.0f, 294.0f, x, y);
  Spr(2656.5f, 579.0f, 414.0f, 181.5f, x, y + 0xc4);
  const int cursor = ScrWork[SW_MailFolder];
  if (cursor != 0xFF) Spr(2434.5f, 1012.5f, 21.0f, 36.0f, x + 0x10, y + 0x3e + cursor * 24 - 1);
  Text(x + 0x24, y + 0x3e, UiStrBase + 0, cursor == 0 ? 0xFF1111 : 0, 20);
  Text(x + 0x24, y + 0x56, UiStrBase + 1, cursor == 1 ? 0xFF1111 : 0, 20);
  if (cursor != 0xFF)
    TextLines(x + 0x12, y + 0xe0, 0xf0, UiStrBase + 0x2a + cursor, 0, 2, 0xFFFFFF, 20);
}

// 0x451650 / 0x452220: sent / received mail list
static void DrawMailList(float x, float y, bool inbox) {
  const auto& list = inbox ? InboxList : SentList;
  Header(inbox ? 0 : 1, x, y);
  const float y0 = y + 0x1d;
  Rect(x, y0, 414.0f, 432.0f, 0xFFFFFF);
  if (list.empty()) {
    Notice(2, x, y, 0);
    return;
  }
  const int cursor = ScrWork[SW_MailCursor];
  if (cursor != 0xFF) Rect(x, y0 + cursor * 24, 414.0f, 36.0f, 0xFFBA95);
  const int first = ScrWork[SW_MailPage] * 12;
  for (int i = 0; i < 12 && first + i < (int)list.size(); i++) {
    const int mail = (int)list[first + i];
    const float rowY = y0 + i * 24;
    float iconX = 616.5f;  // sent, or unread
    if (inbox) {
      if (GetMailStatus(mail, 4)) iconX = 694.5f;
      else if (GetMailStatus(mail, 1)) iconX = 655.5f;
    }
    Spr(iconX, 577.5f, 36.0f, 36.0f, x, rowY);
    if (HasAttachment(mail)) Spr(733.5f, 577.5f, 36.0f, 36.0f, x + 0x60, rowY);
    const MailRecord& m = Mails[mail];
    Ascii(x + 0x18, rowY, TwoDigits(m.Date / 100) + "/" + TwoDigits(m.Date % 100), 0, 24);
    float tx = x + 0x18 + 0x48 + (HasAttachment(mail) ? 0x18 : 0);
    TextLines(tx, rowY, x + 0x116 - tx, MailStr(mail, 1), 0, 1, 0, 24);
  }
  Ascii(1136, 154,
        TwoDigits(ScrWork[SW_MailPage] + 1) + "/" + TwoDigits(ScrWork[SW_MailPages] + 1),
        0xFFFFFF, 16);
  Spr(2239.5f, 465.0f, 414.0f, 432.0f, x, y0);
}

// Mail page layout shared by the viewers (0x452750 / 0x452ef0), the link
// mail (0x450c90) and the reply preview (0x450190): optional date line, To /
// From line, subject (wrapped at 254) with a rule, then the body (276) in 12
// lines scrolled by `scroll`.
struct MailPage {
  int Header;
  bool DateLine;
  float ToIconX;  // 2278.5 "frm", 2317.5 "To"
  bool Links;     // body links selectable (0x445640)
};

// Which body lines a scroll position shows: first body line and line count.
static void BodyWindow(int mail, int scroll, bool dateLine, int& skip, int& count) {
  const int h = dateLine ? 2 : 1;
  const int subj = std::max(1, LineCount(MailStr(mail, 0), 0xfe, 24));
  count = 12;
  if (dateLine && scroll == 0) count--;
  if (scroll <= h - 1) count--;
  if (scroll <= subj + h) count -= subj - std::max(0, scroll - h);
  skip = scroll < subj + h ? 0 : scroll - subj - h;
}

// Link ids shown in the body at this scroll position (0x442580), in order.
static std::vector<int> VisibleLinks(int mail, int scroll) {
  int skip, count;
  BodyWindow(mail, scroll, true, skip, count);
  const std::vector<PhoneLine> lines = WrapText(MailStr(mail, 2), 0x116, 24, 0);
  std::array<bool, 8> seen{};
  for (int i = skip; i < (int)lines.size() && i < skip + count; i++)
    for (const PhoneGlyph& g : lines[i])
      if (g.Link >= 0 && g.Link < 8) seen[g.Link] = true;
  std::vector<int> out;
  for (int i = 0; i < 8; i++)
    if (seen[i]) out.push_back(i);
  return out;
}

// Screen rectangles of the visible link glyphs (mouse picking in screen 27).
static std::vector<LinkBox> LinkBoxes(int mail, int scroll, float x, float y0) {
  int skip, count;
  BodyWindow(mail, scroll, true, skip, count);
  const std::vector<PhoneLine> lines = WrapText(MailStr(mail, 2), 0x116, 24, 0);
  std::vector<LinkBox> out;
  float ty = y0 + (12 - count) * 24;
  for (int i = skip; i < (int)lines.size() && i < skip + count; i++, ty += 24) {
    float dx = x + 2;
    for (const PhoneGlyph& g : lines[i]) {
      if (g.Link >= 0) out.push_back({g.Link, RectF(dx, ty, g.Advance, 24)});
      dx += g.Advance;
    }
  }
  return out;
}

static void DrawMailPage(float x, float y, int mail, int scroll, int lines,
                         const MailPage& page) {
  if (mail < 0 || mail >= (int)Mails.size()) return;
  const MailRecord& m = Mails[mail];
  Header(page.Header, x, y);
  const float y0 = y + 0x1d;
  Rect(x, y0, 414.0f, 432.0f, 0xFFFFFF);
  const int h = page.DateLine ? 2 : 1;
  float ty = y0;
  if (page.DateLine && scroll == 0) {
    Spr(2239.5f, 1012.5f, 36.0f, 36.0f, x, ty);
    if (HasAttachment(mail)) Spr(2395.5f, 1012.5f, 36.0f, 36.0f, x + 0xfc, ty);
    Ascii(x + 0x18, ty,
          TwoDigits(m.Date / 100) + "/" + TwoDigits(m.Date % 100) + " " +
              TwoDigits(m.Time / 100) + ":" + TwoDigits(m.Time % 100),
          0, 24);
    ty += 24;
  }
  if (scroll <= h - 1) {
    Spr(page.ToIconX, 1012.5f, 36.0f, 36.0f, x, ty);
    TextLines(x + 0x18, ty, 0xfc, MailStr(mail, 1), 0, 1, 0, 24);
    ty += 24;
  }
  const int subj = std::max(1, LineCount(MailStr(mail, 0), 0xfe, 24));
  if (scroll <= subj + h) {
    const int skip = std::max(0, scroll - h);
    if (scroll <= h) Spr(2356.5f, 1012.5f, 36.0f, 36.0f, x, ty);
    const int n = TextLines(x + 0x18, ty, 0xfc, MailStr(mail, 0), skip, subj - skip, 0, 24);
    ty += n * 24;
    Rect(x, ty - 1, 414.0f, 1.5f, 0x000000);
  }
  int bodySkip, bodyCount;
  BodyWindow(mail, scroll, page.DateLine, bodySkip, bodyCount);
  if (page.Links && GetMailStatus(mail, 4) == 0)
    TextLines(x, ty, 0x114, MailStr(mail, 2), bodySkip, bodyCount, 0, 24,
              SelectedLink, 0x0000FF, 0xFFFFFF);
  else
    TextLines(x, ty, 0x114, MailStr(mail, 2), bodySkip, bodyCount, 0, 24);
  if (lines > 12) {
    if (scroll > 0) Spr(2239.5f, 943.5f, 414.0f, 13.5f, x, y0);
    if (scroll + 12 < lines) Spr(2239.5f, 960.0f, 414.0f, 13.5f, x, y0 + 0x117);
  }
  Spr(2239.5f, 465.0f, 414.0f, 432.0f, x, y0);
}

static void DrawMailView(float x, float y, bool inbox) {
  DrawMailPage(x, y, ScrWork[SW_MailSelected], ScrWork[6813], ScrWork[SW_MailLines],
               {inbox ? 2 : 3, true, inbox ? 2278.5f : 2317.5f, false});
}

static void DrawLinkMail(float x, float y) {
  DrawMailPage(x, y, ScrWork[SW_MailSelected], ScrWork[6813], ScrWork[SW_MailLines],
               {2, true, 2278.5f, true});
}

static void DrawReplyPreview(float x, float y) {
  DrawMailPage(x, y, ScrWork[6817], ScrWork[6818], ScrWork[6819],
               {10, false, 2317.5f, false});
}

// "Sending mail" / "Mail Sent!" panel with the blinking envelope (0x453950 /
// 0x450840)
static int BlinkCounter = 0;  // 0x18ffbb0
static void DrawSendPanel(float x, float y, int str) {
  DrawReplyPreview(x, y);
  Spr(2239.5f, 1.5f, 414.0f, 247.5f, x, y + 0x4c);
  BlinkCounter = (BlinkCounter + 1) & 0x3f;
  const int sx = (BlinkCounter > 0x1f ? 0x614 : 0x5d4) + 1;
  Spr(sx * 1.5f, 1320.0f, 93.0f, 99.0f, x + 0x69, y + 0x50);
  TextCentered(x, y + 0x96, UiStrBase + str, 0xFF1111, 24, 0xfe);
}

// Reply / Attachments popup choice (0x44f7d0, 0x4514b0)
static void OptionsPopup(bool canReply, bool attachment, float x, float y) {
  const int cursor = ScrWork[6816];
  if (canReply && attachment)
    Popup(0xa, 0xb, cursor, x, y + 0x1d);
  else if (canReply)
    Popup(0xa, 0x270F, cursor, x, y + 0x1d);
  else if (attachment)
    Popup(0xb, 0x270F, cursor, x, y + 0x1d);
}

// 0x44dc00: standby screen, with the incoming call or new mail panel
static void DrawHome(float x, float y) {
  DrawWallpaper(x, y);
  if (GetFlag(SF_Calling)) {
    const int contact = ScrWork[SW_HomeContact];
    Spr(2239.5f, 1.5f, 414.0f, 247.5f, x, y + 0x4c);
    // Caller's number in small keypad digits
    const std::vector<uint16_t> keys = Glyphs(ContactStrBase);
    const std::vector<uint16_t> number = Glyphs(ContactStrBase + contact * 2 + 2);
    float dx = x + 0x48;
    for (uint16_t glyph : number) {
      const auto it = std::find(keys.begin(), keys.end(), glyph);
      if (it != keys.end())
        Spr(((it - keys.begin()) * 18 + 0x5d5) * 1.5f, 976.5f, 24.0f, 33.0f, dx, y + 0xbe);
      dx += 0x10;
    }
    // 0x45a730 (0): ringing handset, blinking
    BlinkCounter = (BlinkCounter + 1) & 0x3f;
    Spr(((BlinkCounter > 0x1f ? 0x60c : 0x5d4) + 1) * 1.5f, 1422.0f, 81.0f, 112.5f,
        x + 0x32, y + 0x50);
    TextLines(x + 0x18, y + 0xa2, 0x108, ContactStrBase + contact * 2 + 1, 0, 2, 0, 20);
    TextLines(x + 0x6a, y + 0x6e, 0xc0, UiStrBase + 0xd, 0, 2, 0xFF1111, 32);
    return;
  }
  if (GetFlag(SF_NewMail)) {
    Spr(2239.5f, 1.5f, 414.0f, 247.5f, x, y + 0x4c);
    // 0x45a730 (1): envelope, blinking
    BlinkCounter = (BlinkCounter + 1) & 0x3f;
    Spr(((BlinkCounter > 0x1f ? 0x614 : 0x5d4) + 1) * 1.5f, 1320.0f, 93.0f, 99.0f,
        x + 0x69, y + 0x50);
    TextCentered(x, y + 0x96, UiStrBase + 9, 0xFF1111, 24, 0xfe);
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

// --- Settings screens -------------------------------------------------------
// 0x44e440: settings menu
static void DrawSettings(float x, float y) {
  Spr(2656.5f, 1060.5f, 414.0f, 75.0f, x, y);
  Spr(2656.5f, 838.5f, 414.0f, 219.0f, x, y + 0x32);
  Spr(2656.5f, 579.0f, 414.0f, 181.5f, x, y + 0xc4);
  const int cursor = ScrWork[SW_SettingsCursor];
  if (cursor != 0xFF) Spr(2434.5f, 1012.5f, 21.0f, 36.0f, x + 0x10, y + 0x3e + cursor * 24 - 1);
  for (int i = 0; i < 3; i++)
    Text(x + 0x24, y + 0x3e + 24 * i, UiStrBase + 4 + i, cursor == i ? 0xFF1111 : 0, 20);
  if (cursor != 0xFF)
    TextLines(x + 0x12, y + 0xe0, 0xf0, UiStrBase + 0x2d + cursor, 0, 2, 0xFFFFFF, 20);
}

static void SprSheet(const char* name, float sx, float sy, float sw, float sh,
                     float dx, float dy) {
  const auto it = Profile::SpriteSheets.find(name);
  if (it == Profile::SpriteSheets.end()) return;
  Sprite spr(it->second, sx / 1.5f, sy / 1.5f, sw / 1.5f, sh / 1.5f);
  Renderer->DrawSprite(spr, RectF(dx, dy, sw / 1.5f, sh / 1.5f),
                       glm::vec4(1.0f, 1.0f, 1.0f, Alpha));
}

// 0x44e740: wallpaper thumbnails (PWCGTHUM, 12 per row) and names
static void DrawWallpaperList(float x, float y) {
  Header(5, x, y);
  const float y0 = y + 0x1d;
  Rect(x, y0, 414.0f, 432.0f, 0xFFFFFF);
  if (WallCursor != 0xFF) Rect(x, y0 + WallCursor * 48, 414.0f, 72.0f, 0xFFBA95);
  const int count = (int)WallpaperList.size();
  for (int i = 0; i < 6; i++) {
    const int idx = WallPage * 6 + i;
    if (idx >= count) break;
    const int id = (int)WallpaperList[idx];
    const float rowY = y0 + 48.0f * i;
    Rect(x, rowY + 0x2f - 0.2f, 414.0f, 2.1f, 0x808080);
    SprSheet("PhoneWallpaperThumb", ((id % 12) * 42 + 1) * 1.5f,
             ((id / 12) * 48 + 1) * 1.5f, 61.5f, 70.5f, x + 9, rowY);
    if (ScrWork[SW_WallpaperSet] == id)
      Spr(2562.0f, 1425.0f, 61.5f, 70.5f, x + 9, rowY);
    TextLines(x + 0x3c, rowY, 0xd8, UiStrBase + Wallpapers[id].Name, 0, 2, 0, 24);
  }
  Ascii(1136, 154, TwoDigits(WallPage + 1) + "/" + TwoDigits(WallPages),
        WallHover ? 0xFFEE88 : 0xFFFFFF, 16);
  Spr(2239.5f, 465.0f, 414.0f, 432.0f, x, y0);
}

// 0x44eb70 / 0x44ee70: ringtone / mail tone list ("none" first)
static void DrawToneList(float x, float y, bool mail) {
  Header(mail ? 7 : 6, x, y);
  const float y0 = y + 0x1d;
  Rect(x, y0, 414.0f, 432.0f, 0xFFFFFF);
  const int cursor = ScrWork[SW_ToneCursor];
  if (cursor != 0xFF) Rect(x, y0 + cursor * 24, 414.0f, 36.0f, 0xFFBA95);
  const int value = ScrWork[mail ? SW_MailTone : SW_Ringtone];
  if (value == 0xFFFF) {
    Spr(2491.5f, 1012.5f, 27.0f, 36.0f, x, y0);
  } else {
    for (size_t i = 0; i < SoundList.size(); i++) {
      if ((int)ToneValue(SoundList[i], mail) == value) {
        Spr(2461.5f, 1012.5f, 27.0f, 36.0f, x, y0 + 24.0f * (i + 1));
        break;
      }
    }
  }
  Text(x + 0x14, y0, UiStrBase + 0x17, 0, 24);
  for (size_t i = 0; i < SoundList.size(); i++)
    TextLines(x + 0x14, y0 + 24.0f * (i + 1), 0xfe,
              UiStrBase + Sounds[SoundList[i]].Name, 0, 1, 0, 24);
  Spr(2239.5f, 465.0f, 414.0f, 432.0f, x, y0);
}

// 0x451b80: music player for the ringtone preview
static void DrawTonePreview(float x, float y) {
  Spr(877.5f, 1293.0f, 297.0f, 45.0f, x + 0x24, y + 0x3c);
  DrawDots(x + 0x12, y + 0x26, 20);
  // Title, scrolling one glyph every 8 frames when wider than 0xdc
  const uint32_t name = UiStrBase + Sounds[PreviewSound].Name;
  const std::vector<uint16_t> glyphs = Glyphs(name);
  const Font& font = *Profile::Dialogue::DialogueFont;
  const float size = 24;
  float width = 0;
  for (uint16_t g : glyphs) width += font.GetAdvanceWidth(g) * size / font.BitmapEmWidth;
  size_t first = 0;
  if (width > 0xdc) {
    if (MarqueeTimer > 0) {
      MarqueeTimer--;
    } else {
      MarqueeOffset++;
      MarqueeTimer = 8;
    }
    if (MarqueeOffset >= (int)glyphs.size()) {
      MarqueeOffset = 0;
      MarqueeTimer = 0x78;
    }
    first = (size_t)MarqueeOffset;
  }
  std::vector<ProcessedTextGlyph> out;
  float dx = x + 0x18;
  for (size_t i = first; i < glyphs.size(); i++) {
    const float adv = font.GetAdvanceWidth(glyphs[i]) * size / font.BitmapEmWidth;
    if (dx + adv > x + 0x18 + 0xdc) break;
    out.push_back(font.PlaceGlyph(glyphs[i], {dx, y + 0x94}, size,
                                  DialogueColorPair{0xFFFFFF, 0}, 1.0f));
    dx += adv;
  }
  Profile::Dialogue::DialogueFont->DrawProcessedText(out, Alpha);
  Spr(2239.5f, 1060.5f, 414.0f, 75.0f, x, y + 0xec);
  // Elapsed time as HH:MM:SS in keypad digits
  const int secs = (int)((SDL_GetTicks() - PreviewStart) / 1000 % 360000);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", (secs / 3600) % 100,
                (secs / 60) % 60, secs % 60);
  for (int i = 0; i < 8; i++) {
    const float sx = buf[i] == ':' ? 0x29f * 1.5f : ((buf[i] - '0') * 24 + 0x14f) * 1.5f;
    Spr(sx, 1366.5f, 33.0f, 45.0f, x + 0x28 + 24.0f * i, y + 0xd6);
  }
}

static void DrawScreen(int mode, float x, float y) {
  switch (mode) {
    case 0:
      DrawHome(x, y);
      break;
    case 1:
      DrawMailFolders(x, y);
      break;
    case 2:
      DrawMailList(x, y, false);
      break;
    case 3:
      DrawMailList(x, y, true);
      break;
    case 4:
      DrawMailView(x, y, false);
      break;
    case 5:
      DrawMailView(x, y, true);
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
    case 9:
      DrawSendPanel(x, y, 0xe);
      break;
    case 10:
      DrawCalling(x, y);
      break;
    case 11:  // connected: "Voice Only" (0x44e2c0)
      Rect(x, y, 414.0f, 468.0f, 0x000000);
      Spr(502.5f, 1293.0f, 276.0f, 54.0f, x + 0x2e, y + 0x88);
      break;
    case 23: {  // keypad: connected screen + typed digits (0x450620)
      Rect(x, y, 414.0f, 468.0f, 0x000000);
      Spr(502.5f, 1293.0f, 276.0f, 54.0f, x + 0x2e, y + 0x88);
      const int count = std::clamp(ScrWork[SW_KeypadCount], 0, 4);
      for (int i = 0; i < count; i++) {
        const float dx = x - 24.0f * count + 0x10e + 24.0f * i;
        Spr((KeypadInput[i] * 24 + 0x14f) * 1.5f, 1366.5f, 33.0f, 45.0f, dx,
            y + 0xd4);
      }
    } break;
    case 12:  // 0x44e3a0
      Spr(616.5f, 109.5f, 414.0f, 468.0f, x, y);
      break;
    case 19: {
      const int mail = ScrWork[SW_MailSelected];
      DrawMailView(x, y, ScrWork[SW_MailFolder] == 0);
      OptionsPopup(CanReplyFromView(mail), HasAttachment(mail), x, y);
    } break;
    case 20:
    case 21:
      DrawMenu(x, y);
      break;
    case 13:
      DrawSettings(x, y);
      break;
    case 14:
      DrawWallpaperList(x, y);
      break;
    case 15:
    case 16:
      DrawToneList(x, y, mode == 16);
      break;
    case 29:  // 0x4515e0
      DrawToneList(x, y, !RingtoneScreen());
      if (ScrWork[SW_ToneCursor] != 0)
        Popup(0x15, 0x16, ScrWork[SW_SetPopup], x, y + 0x1d);
      else
        Popup(0x16, 0x270F, ScrWork[SW_SetPopup], x, y + 0x1d);
      break;
    case 30:  // 0x451b20
      DrawToneList(x, y, !RingtoneScreen());
      Notice(RingtoneScreen() ? 0x22 : 0x33, x, y + 0x1d, 0);
      break;
    case 31:
      DrawTonePreview(x, y);
      break;
    case 35:  // 0x452070
      DrawWallpaperList(x, y);
      Popup(0x4c, 0x16, ScrWork[SW_SetPopup], x, y + 0x1d);
      break;
    case 36:  // 0x4520b0
      DrawWallpaperList(x, y);
      Notice(0x4b, x, y + 0x1d, 0);
      break;
    case 37:  // 0x45c4a0
      DrawWallpaper(x, y);
      break;
    case 206:
      DrawTonePreview(x, y);
      break;
    case 200: case 201: case 202: case 203: case 204: case 205:
    case 207: case 208: case 209:  // 0x44fe70: the mail behind
      if (ReturnMode == 5)
        DrawMailView(x, y, true);
      else if (ReturnMode == 27)
        DrawLinkMail(x, y);
      break;
    case 22:
      DrawReplyPreview(x, y);
      break;
    case 24:
      DrawSendPanel(x, y, 0xc);
      break;
    case 27:
      DrawLinkMail(x, y);
      break;
    case 28: {
      const int mail = ScrWork[SW_MailSelected];
      DrawLinkMail(x, y);
      OptionsPopup(CanReplyByLink(mail), HasAttachment(mail), x, y);
    } break;
    case 40:
      DrawReplyPreview(x, y);
      Popup(0x36, 0x270F, ScrWork[SW_MailPopupCursor], x, y + 0x1d);
      break;
    case 34:  // 0x452030: preview + Send/Save
      DrawReplyPreview(x, y);
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

// Keypad overlay at a fixed screen position, faded by SW 6815 (0x450620).
static void DrawKeypad() {
  const float alpha = std::clamp(ScrWork[SW_KeypadAlpha], 0, 256) / 256.0f;
  Spr(1033.5f, 367.5f, 502.5f, 369.0f, 708 / 1.5f, 342 / 1.5f, alpha);
  const int key = ScrWork[SW_KeypadCursor];
  if (key == 0xFF || key < 0 || key > 11 || ScrWork[SW_KeypadAlpha] < 0xFF)
    return;
  const int col = key % 3, row = key / 3;
  Spr((col * 77 + 0x2e6) * 1.5f, (row * 47 + 0x21c) * 1.5f, 112.5f, 67.5f,
      (float)(col * 77 + 0x20d), (float)(row * 47 + 0x114), alpha);
}

// --- Attachment overlays (layer SW 6842 / 6835, 0x428790) -------------------
// PHONE2.DDS holds the full-screen phone frame and its status bar; PACG.DDS
// the attachment pictures (6 per row, 681x770).
static bool AnyUnreadMail() {  // 0x44db60
  for (uint32_t i = 0; i < Mails.size(); i++)
    if (!(Mails[i].Flags & 1) && GetMailStatus(i, 0) && !GetMailStatus(i, 1))
      return true;
  return false;
}
static void DrawBigFrame(float alpha) {
  const float saved = Alpha;
  Alpha = alpha;
  SprSheet("PhoneFrame", 0, 0, 1920, 1080, 0, 0);
  SprSheet("PhoneFrame", 1.5f, GetFlag(SF_HasSignal) ? 1081.5f : 1155.0f, 828,
           70.5f, 546 / 1.5f, 0);
  if (AnyUnreadMail())
    SprSheet("PhoneFrame", 832.5f, 1081.5f, 84, 48, 1134 / 1.5f, 0);
  Alpha = saved;
}
static void DrawSlide(float alpha) {  // 0x44fc90
  DrawBigFrame(alpha);
  const int id = GetFlag(1607) ? ScrWork[SW_SlideB] : ScrWork[SW_SlideA];
  if (id < 0 || id >= 36) return;
  const auto it = Profile::SpriteSheets.find("PhoneAttach");
  if (it == Profile::SpriteSheets.end()) return;
  Sprite spr(it->second, (id % 6) * 681 / 1.5f, (id / 6) * 770 / 1.5f,
             680 / 1.5f, 768 / 1.5f);
  Renderer->DrawSprite(spr, RectF(546 / 1.5f, 70.5f / 1.5f, 828 / 1.5f, 936 / 1.5f),
                       glm::vec4(1.0f, 1.0f, 1.0f, alpha));
}
// 0x44d430: document text, 31 lines (25 for full-screen documents >= 1000)
static void DrawDocument() {
  const int fade = ScrWork[SW_DocFade];
  if (fade <= 0 || !DocBuffer) return;
  const float alpha = std::min(fade, 256) / 256.0f;
  const int doc = ScrWork[SW_Doc] % 1000;
  const bool framed = ScrWork[SW_Doc] < 1000;
  float x = 0xe8, y = 0x9e, width = 0x320;
  int maxLines = 0x19;
  if (framed) {
    DrawBigFrame(alpha);
    Renderer->DrawQuad(RectF(546 / 1.5f, 70.5f / 1.5f, 828 / 1.5f, 936 / 1.5f),
                       glm::vec4(1.0f, 1.0f, 1.0f, alpha));
    x = 0x16d, y = 0x30, width = 0x228, maxLines = 0x1f;
  }
  const auto& buf = ScriptBuffers[DocBuffer];
  const uint32_t label = ReadU16(buf.data() + DocTable + doc * 2);
  const uint8_t* p = buf.data() + ScriptGetLabelAddress(DocBuffer, label);
  int skip = ScrWork[SW_DocScroll], drawn = 0;
  const float saved = Alpha;
  Alpha = alpha;
  for (uint16_t str = ReadU16(p); str != 0xFFFF && drawn < maxLines;
       p += 2, str = ReadU16(p)) {
    if (skip > 0) {
      skip--;
      continue;
    }
    Sc3VmThread dummy;
    dummy.ScriptBufferId = DocBuffer;
    dummy.IpOffset = ScriptGetStrAddress(DocBuffer, str);
    std::vector<ProcessedTextGlyph> glyphs = TextLayoutPlainLine(
        &dummy, 255, *Profile::Dialogue::DialogueFont, 20,
        DialogueColorPair{framed ? 0u : 0xFFFFFFu, 0}, 1.0f,
        glm::vec2(x, y + 20.0f * drawn), TextAlignment::Left);
    (void)width;
    Profile::Dialogue::DialogueFont->DrawProcessedText(glyphs, Alpha);
    drawn++;
  }
  if (framed) {
    const auto arrow = [&](float sy, float dy, bool hot) {
      const auto it = Profile::SpriteSheets.find("PhoneFrame");
      if (it == Profile::SpriteSheets.end()) return;
      Sprite spr(it->second, 2 / 1.5f, sy / 1.5f, 828 / 1.5f, 24 / 1.5f);
      Renderer->DrawSprite(spr, RectF(546 / 1.5f, dy / 1.5f, 828 / 1.5f, 24 / 1.5f),
                           Rgb(hot ? 0xFFEE88 : 0xFFFFFF, alpha));
    };
    if (ScrWork[SW_DocScroll] > 0) arrow(1228, 69.5f, DocHover == 1);
    if (ScrWork[SW_DocMaxScroll] > ScrWork[SW_DocScroll])
      arrow(1254, 982.5f, DocHover == 2);
  }
  Alpha = saved;
}

// 0x453b10
static void Render() {
  const int ct = ScrWork[SW_PhoneCt];
  if (ct == 0) return;
  // 0x45a670: the default handset turns into view (PHONE3.DDS, 7 frames a
  // row, 2 ticks each) instead of sliding in.
  if (ScrWork[SW_PhoneColor] == 0 && ct < 20) {
    const auto it = Profile::SpriteSheets.find("PhoneOpen");
    if (it == Profile::SpriteSheets.end()) return;
    const int row = ct / 14, col = (ct - 14 * row) / 2;
    Sprite spr(it->second, (float)(col * 0x124 + 1), (float)(row * 0x1f9 + 1),
               436.5f / 1.5f, 756.0f / 1.5f);
    Renderer->DrawSprite(spr, RectF(1248 / 1.5f, 0, 624 / 1.5f, 1080 / 1.5f),
                         glm::vec4(1.0f));
    return;
  }
  const int left = 0x47a - 14 * ct;
  Alpha = std::min(ct * 256 / 20, 256) / 256.0f;
  // Handset
  Spr(1.5f, 1.5f, 567.0f, 1077.0f, (float)(left - 10), 2.0f);
  const float x = (float)(left + 0x29);
  DrawScreen(ScrWork[SW_PhoneMode], x, 0x91);
  if (ScrWork[SW_PhoneMode] != 0x63) {
    // Status bar (0x44d830): signal / out of range
    Spr(2656.5f, GetFlag(SF_HasSignal) ? 1.5f : 49.5f, 414.0f, 45.0f, x, 0x73);
    if (AnyUnreadMail()) Spr(835.5f, 1291.5f, 40.5f, 28.5f, x + 0xc4, 178.5f / 1.5f);
  }
  // Screen glass
  Spr(616.5f, 631.5f, 414.0f, 447.0f, x, 0x73);
  if (ScrWork[SW_PhoneMode] == 23) DrawKeypad();
}

void RenderLayer(uint32_t layer) {
  if ((int)layer == ScrWork[SW_PhonePri]) {
    if (ScrWork[SW_PhoneHide1] < 0xFF && ScrWork[SW_PhoneHide2] < 0xFF) Render();
    // Movie attachments only show the frame: Bink playback isn't supported.
    if (ScrWork[SW_AttachFade] > 0)
      DrawBigFrame(std::min(ScrWork[SW_AttachFade], 256) / 256.0f);
    if (ScrWork[SW_SlideFade] > 0)
      DrawSlide(std::min(ScrWork[SW_SlideFade], 256) / 256.0f);
  }
  if ((int)layer == ScrWork[SW_DocLayer]) DrawDocument();
}

// 10:38 (0x468710): sub-op 0 registers the document table from _ATCH.SCX;
// 1-6 add mouse wheel / arrow scrolling to a script-driven text screen
// (flags 1620/1621 or 1622/1623).
void ScrollInstruction(Sc3VmThread* thread) {
  StartInstruction;
  PopUint8(type);
  switch (type) {
    case 0: {
      PopUint16(label);
      DocBuffer = thread->ScriptBufferId;
      DocTable = ScriptGetLabelAddress(thread->ScriptBufferId, label);
    } break;
    case 1:
    case 4:
      SetFlag(type == 1 ? 1620 : 1622, 0);
      SetFlag(type == 1 ? 1621 : 1623, 0);
      SetFlag(3002, 1);
      break;
    case 2:
    case 5: {
      const int upFlag = type == 2 ? 1620 : 1622;
      SetFlag(upFlag, 0);
      SetFlag(upFlag + 1, 0);
      DocHover = 0;
      if (!PointerMode()) break;
      const RectF up(546 / 1.5f, 66 / 1.5f, 828 / 1.5f, 32 / 1.5f);
      const RectF down(546 / 1.5f, 986 / 1.5f, 828 / 1.5f, 32 / 1.5f);
      if (Input::MouseWheelDeltaY > 0) SetFlag(upFlag, 1);
      if (Input::MouseWheelDeltaY < 0) SetFlag(upFlag + 1, 1);
      if (Over(up)) DocHover = 1;
      if (Over(down)) DocHover = 2;
      if (Clicked(up)) {
        SetFlag(upFlag, 1);
      } else if (Clicked(down)) {
        SetFlag(upFlag + 1, 1);
      } else if (PADinputMouseWentDown & PAD1A) {
        PADinputButtonWentDown |= PADcustom[5];
      }
    } break;
    case 3:
    case 6:
      SetFlag(type == 3 ? 1620 : 1622, 0);
      SetFlag(type == 3 ? 1621 : 1623, 0);
      SetFlag(3002, 0);
      break;
    default:
      break;
  }
}

}  // namespace Phone
}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
