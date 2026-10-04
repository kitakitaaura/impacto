#include "titlemenu.h"
#include "../../profile_internal.h"

#include "../../ui/titlemenu.h"
#include "../../../game.h"
#include "../../../ui/ui.h"
#include "../../../games/sghd/titlemenu.h"

namespace Impacto {
namespace Profile {
namespace SGHD {
namespace TitleMenu {

// Items = { { Id, Sprite, HighlightSprite, Sub = { { Id, Sprite, ... } } } }
static std::vector<SubItem> ParseSubItems() {
  std::vector<SubItem> items;
  if (!TryPushMember("Sub")) return items;
  AssertIs(LUA_TTABLE);
  ForEachProfileArray([&](uint32_t index) {
    if (items.size() <= index) items.resize(index + 1);
    items[index] = {EnsureGetMember<int>("Id"),
                    EnsureGetMember<Sprite>("Sprite"),
                    EnsureGetMember<Sprite>("HighlightSprite")};
  });
  Pop();
  return items;
}

void Configure() {
  BackgroundSprite = EnsureGetMember<Sprite>("BackgroundSprite");
  BackgroundPos = EnsureGetMember<glm::vec2>("BackgroundPos");
  ArrowSprite = EnsureGetMember<Sprite>("ArrowSprite");
  ArrowOffset = EnsureGetMember<glm::vec2>("ArrowOffset");
  MainItemPos = EnsureGetMember<glm::vec2>("MainItemPos");
  MainItemSpacing = EnsureGetMember<float>("MainItemSpacing");
  SubItemOffset = EnsureGetMember<glm::vec2>("SubItemOffset");
  SubItemSpacing = EnsureGetMember<float>("SubItemSpacing");
  FadeDuration = EnsureGetMember<float>("FadeDuration");

  MainItems.clear();
  EnsurePushMemberOfType("Items", LUA_TTABLE);
  ForEachProfileArray([&](uint32_t index) {
    if (MainItems.size() <= index) MainItems.resize(index + 1);
    MainItems[index] = {EnsureGetMember<int>("Id"),
                        EnsureGetMember<Sprite>("Sprite"),
                        EnsureGetMember<Sprite>("HighlightSprite"),
                        ParseSubItems()};
  });
  Pop();

  auto drawType = EnsureGetMember<Game::DrawComponentType>("DrawType");
  UI::TitleMenuPtr = new UI::SGHD::TitleMenu();
  UI::Menus[drawType].push_back(UI::TitleMenuPtr);
}

}  // namespace TitleMenu
}  // namespace SGHD
}  // namespace Profile
}  // namespace Impacto
