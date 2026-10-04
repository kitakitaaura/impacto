#pragma once

#include <vector>
#include "../../../spritesheet.h"

namespace Impacto {
namespace Profile {
namespace SGHD {
namespace TitleMenu {

void Configure();

struct SubItem {
  int Id;
  Sprite Normal;
  Sprite Highlighted;
};

struct MainItem {
  int Id;  // -1: opens the submenu instead of choosing
  Sprite Normal;
  Sprite Highlighted;
  std::vector<SubItem> SubItems;
};

inline Sprite BackgroundSprite;
inline glm::vec2 BackgroundPos;
inline Sprite ArrowSprite;
inline glm::vec2 ArrowOffset;

inline std::vector<MainItem> MainItems;
inline glm::vec2 MainItemPos;
inline float MainItemSpacing;
inline glm::vec2 SubItemOffset;
inline float SubItemSpacing;

inline float FadeDuration;

}  // namespace TitleMenu
}  // namespace SGHD
}  // namespace Profile
}  // namespace Impacto
