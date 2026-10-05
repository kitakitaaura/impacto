#include "savemenu.h"
#include "../../profile_internal.h"

#include "../../../game.h"
#include "../../../ui/ui.h"
#include "../../../games/sghd/savemenu.h"

namespace Impacto {
namespace Profile {
namespace SGHD {
namespace SaveMenu {

void Configure() {
  SaveBackground = EnsureGetMember<Sprite>("SaveBackground");
  LoadBackground = EnsureGetMember<Sprite>("LoadBackground");
  QuickLoadBackground = EnsureGetMember<Sprite>("QuickLoadBackground");
  SlotSprite = EnsureGetMember<Sprite>("SlotSprite");
  NoDataSprite = EnsureGetMember<Sprite>("NoDataSprite");
  CursorSprite = EnsureGetMember<Sprite>("CursorSprite");
  GetMemberArray<Sprite>(std::span(DigitSprites, 10), "DigitSprites");
  PageTotalSprite = EnsureGetMember<Sprite>("PageTotalSprite");

  SlotsPos = EnsureGetMember<glm::vec2>("SlotsPos");
  SlotSpacing = EnsureGetMember<glm::vec2>("SlotSpacing");
  Columns = EnsureGetMember<int>("Columns");
  Rows = EnsureGetMember<int>("Rows");
  Pages = EnsureGetMember<int>("Pages");
  ThumbnailOffset = EnsureGetMember<glm::vec2>("ThumbnailOffset");
  ThumbnailSize = EnsureGetMember<glm::vec2>("ThumbnailSize");
  SlotNumberOffset = EnsureGetMember<glm::vec2>("SlotNumberOffset");
  SlotNumberScale = EnsureGetMember<float>("SlotNumberScale");
  PagePos = EnsureGetMember<glm::vec2>("PagePos");

  auto drawType = EnsureGetMember<Game::DrawComponentType>("DrawType");
  UI::SaveMenuPtr = new UI::SGHD::SaveMenu();
  UI::Menus[drawType].push_back(UI::SaveMenuPtr);
}

}  // namespace SaveMenu
}  // namespace SGHD
}  // namespace Profile
}  // namespace Impacto
