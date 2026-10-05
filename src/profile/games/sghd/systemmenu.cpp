#include "systemmenu.h"
#include "../../profile_internal.h"

#include "../../../game.h"
#include "../../../ui/ui.h"
#include "../../../games/sghd/systemmenu.h"

namespace Impacto {
namespace Profile {
namespace SGHD {
namespace SystemMenu {

void Configure() {
  EnsurePushMemberOfType("ItemSprites", LUA_TTABLE);
  ItemSprites.clear();
  ForEachProfileArray([](uint32_t index) {
    if (ItemSprites.size() <= index) ItemSprites.resize(index + 1);
    ItemSprites[index] = EnsureGet<Sprite>();
  });
  Pop();
  FrameSprite = EnsureGetMember<Sprite>("FrameSprite");
  FramePos = EnsureGetMember<glm::vec2>("FramePos");
  ItemsPos = EnsureGetMember<glm::vec2>("ItemsPos");
  ItemSpacing = EnsureGetMember<float>("ItemSpacing");
  UnselectedBrightness = EnsureGetMember<float>("UnselectedBrightness");

  auto drawType = EnsureGetMember<Game::DrawComponentType>("DrawType");
  UI::SystemMenuPtr = new UI::SGHD::SystemMenu();
  UI::Menus[drawType].push_back(UI::SystemMenuPtr);
}

}  // namespace SystemMenu
}  // namespace SGHD
}  // namespace Profile
}  // namespace Impacto
