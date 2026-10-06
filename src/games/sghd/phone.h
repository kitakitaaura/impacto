#pragma once

#include <cstddef>
#include <cstdint>

namespace Impacto {
namespace Vm {
struct Sc3VmThread;
}
namespace UI {
namespace SGHD {

// Okabe's cell phone in STEINS;GATE (Steam). Ported from the game executable:
// the phone's state lives in ScrWork 6801.. (6802 = current screen), scripts
// drive it through instruction 10:37 (Instruction()), and it is drawn at
// render layer ScrWork[6842]. Addresses in comments refer to the Steam
// Game.exe (CoZ-patched, unpacked) the behaviour was read from.
namespace Phone {

void Reset();

// Save data: the 500-byte phone bit array the game stores in every save entry
// (entry offset 0x3EC0). Loading rebuilds the per-mail status from it.
constexpr size_t SaveBitsSize = 500;
void GetSaveBits(uint8_t* out);
void SetSaveBits(const uint8_t* in);

// 10:37 for the SGHD instruction set. Returns after consuming its operands;
// may redirect the thread (jump sub-ops).
void Instruction(Vm::Sc3VmThread* thread);

// Once per 60 Hz system tick (open/close counter, 0x459d80).
void Tick();

void RenderLayer(uint32_t layer);

// 10:38: document table for attachments, mouse scrolling helpers.
void ScrollInstruction(Vm::Sc3VmThread* thread);

// Dev/test: deliver a mail as 10:37:12 would (DevInput "phonemail").
void DevReceiveMail(int slot, int mail);

}  // namespace Phone
}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
