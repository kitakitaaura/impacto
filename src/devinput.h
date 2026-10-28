#pragma once

#include <SDL3/SDL_events.h>

namespace Impacto {
namespace DevInput {

// Dev/test aid: with IMPACTO_DEV_INPUT=<file>, each frame the file (if present)
// is tailed and newly appended commands are applied to this frame's input:
//   pad A|B|X|Y|UP|DOWN|LEFT|RIGHT|START|SELECT|L1|R1|L2|R2
//   move X Y      (design coordinates)
//   click X Y     (left click at X Y)
//   rclick X Y    (right click at X Y)
//   hold NAME / release NAME   (keep a pad button down across frames)
//   sw FROM TO / flag FROM TO   (log ScrWork / flag values)
//   setsw IDX V / setflag IDX V (write a ScrWork / flag value)
//   phonemail SLOT MAIL         (STEINS;GATE: deliver a phone mail)
//   key NAME / keydown NAME / keyup NAME (SDL scancode name: Return, Escape,
//                               Left Ctrl, F3... use _ for spaces)
//   mouse X Y / mclick X Y / mrclick X Y / mmclick X Y  (SDL mouse events
//                               at design coordinates: move, left/right/middle)
//   wheel DY      (SDL wheel event; positive scrolls up)
//   state         (reply one DevState line: script/line, menus, text...)
//   shot PATH     (write this frame as a binary PPM, reply DevShot PATH)
//   echo TEXT     (reply DevEcho TEXT; marks a point in the reply stream)
//   quit
// Replies are appended to <IMPACTO_DEV_INPUT>.out (and logged).
void Apply();
// With IMPACTO_DEV_INPUT_ONLY=1, true for real input events (only the ones
// pushed by key/mouse commands get through).
bool DropEvent(const SDL_Event* ev);
// Called after the frame is rendered, before it is presented.
void AfterRender();

}  // namespace DevInput
}  // namespace Impacto
