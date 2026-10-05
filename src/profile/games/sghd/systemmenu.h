#pragma once

#include <vector>
#include "../../../spritesheet.h"

namespace Impacto {
namespace Profile {
namespace SGHD {
namespace SystemMenu {

void Configure();

inline std::vector<Sprite> ItemSprites;
inline Sprite FrameSprite;
inline glm::vec2 FramePos;
inline glm::vec2 ItemsPos;
inline float ItemSpacing;
inline float UnselectedBrightness;

}  // namespace SystemMenu
}  // namespace SGHD
}  // namespace Profile
}  // namespace Impacto
