#include "systemmenu.h"

#include "../../profile/games/sghd/systemmenu.h"
#include "../../profile/scriptvars.h"
#include "../../renderer/renderer.h"
#include "../../vm/interface/input.h"
#include "../../inputsystem.h"
#include "../../mem.h"

namespace Impacto {
namespace UI {
namespace SGHD {

using namespace Impacto::Profile::SGHD::SystemMenu;
using namespace Impacto::Profile::ScriptVars;
using namespace Impacto::Vm::Interface;

void SystemMenu::Init() {
  ChoiceMade = false;
  Cursor = std::clamp(ScrWork[SW_SYSMENUCNO], 0, (int)ItemSprites.size() - 1);
}

RectF SystemMenu::ItemRect(int i) const {
  const glm::vec2 pos = ItemsPos + glm::vec2(0.0f, i * ItemSpacing);
  return RectF(pos.x, pos.y, ItemSprites[i].ScaledWidth(),
               ItemSprites[i].ScaledHeight());
}

void SystemMenu::Update(float dt) {
  const int ct = ScrWork[SW_SYSMENUCT];
  State = ct <= 0 ? Hidden : ct >= 32 ? Shown : Showing;
  if (State != Shown || ChoiceMade) return;

  const int count = (int)ItemSprites.size();
  if (PADinputButtonWentDown & PAD1DOWN) Cursor = (Cursor + 1) % count;
  if (PADinputButtonWentDown & PAD1UP) Cursor = (Cursor + count - 1) % count;

  int hovered = -1;
  for (int i = 0; i < count; i++) {
    if (ItemRect(i).ContainsPoint(Input::CurMousePos)) hovered = i;
  }
  if (hovered >= 0 && Input::PrevMousePos != Input::CurMousePos)
    Cursor = hovered;
  ScrWork[SW_SYSMENUCNO] = Cursor;

  // Keyboard/pad confirm reaches the script's own key check; a click only
  // counts on an item (a click elsewhere is left to the script, e.g. close).
  if ((PADinputMouseWentDown & PAD1A) && hovered == Cursor) ChoiceMade = true;
}

void SystemMenu::Render() {
  if (State == Hidden) return;
  const float alpha =
      std::clamp(ScrWork[SW_SYSMENUCT] / 32.0f, 0.0f, 1.0f) *
      std::clamp(ScrWork[SW_SYSMENUALPHA] / 256.0f, 0.0f, 1.0f);
  Renderer->DrawSprite(FrameSprite, FramePos, glm::vec4(1.0f, 1.0f, 1.0f, alpha));
  for (int i = 0; i < (int)ItemSprites.size(); i++) {
    const float b = i == Cursor ? 1.0f : UnselectedBrightness;
    Renderer->DrawSprite(ItemSprites[i], ItemRect(i).GetPos(),
                         glm::vec4(b, b, b, alpha));
  }
}

}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
