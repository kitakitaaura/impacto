#pragma once

namespace Impacto {
namespace DevInput {

// Dev/test aid: with IMPACTO_DEV_INPUT=<file>, each frame the file (if present)
// is read, deleted and its commands applied to this frame's input:
//   pad A|B|X|Y|UP|DOWN|LEFT|RIGHT|START|SELECT|L1|R1|L2|R2
//   move X Y      (design coordinates)
//   click X Y     (left click at X Y)
//   rclick X Y    (right click at X Y)
//   hold NAME / release NAME   (keep a pad button down across frames)
//   sw FROM TO / flag FROM TO   (log ScrWork / flag values)
void Apply();

}  // namespace DevInput
}  // namespace Impacto
