#pragma once

#include "../../../spritesheet.h"

namespace Impacto {
namespace Profile {
namespace SGHD {
namespace SaveMenu {

void Configure();

inline Sprite SaveBackground, LoadBackground, QuickLoadBackground;
inline Sprite SlotSprite, NoDataSprite, CursorSprite;
inline Sprite DigitSprites[10];
inline Sprite PageTotalSprite;  // "/6"

inline glm::vec2 SlotsPos;
inline glm::vec2 SlotSpacing;
inline int Columns, Rows, Pages;
inline glm::vec2 ThumbnailOffset;
inline glm::vec2 ThumbnailSize;
inline glm::vec2 SlotNumberOffset;
inline float SlotNumberScale;
inline glm::vec2 PagePos;

}  // namespace SaveMenu
}  // namespace SGHD
}  // namespace Profile
}  // namespace Impacto
