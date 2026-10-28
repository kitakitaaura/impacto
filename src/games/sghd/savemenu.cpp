#include "savemenu.h"

#include "../../profile/games/sghd/savemenu.h"
#include "../../profile/scriptvars.h"
#include "../../renderer/renderer.h"
#include "../../vm/interface/input.h"
#include "../../inputsystem.h"
#include "../../ui/sysmesbox.h"
#include "../../data/savesystem.h"
#include "../../mem.h"

namespace Impacto {
namespace UI {
namespace SGHD {

using namespace Impacto::Profile::SGHD::SaveMenu;
using namespace Impacto::Profile::ScriptVars;
using namespace Impacto::Vm::Interface;

static SaveSystem::SaveType TypeFor(std::optional<SaveMenuPageType> page) {
  return page == SaveMenuPageType::QuickLoad ? SaveSystem::SaveType::Quick
                                             : SaveSystem::SaveType::Full;
}

void SaveMenu::Init() {
  ChoiceMade = false;
  Cursor = 0;
  ActivePage = 0;
  SubMenuNo = ScrWork[SW_SYSSUBMENUNO];
}

void SaveMenu::Show() {
  if (State == Hidden || State == Hiding) State = Showing;
}

void SaveMenu::Hide() {
  if (State == Shown || State == Showing) State = Hiding;
}

RectF SaveMenu::SlotRect(int slot) const {
  const int col = slot % Columns;
  const int row = slot / Columns;
  const glm::vec2 pos = SlotsPos + glm::vec2(col, row) * SlotSpacing;
  return RectF(pos.x, pos.y, SlotSprite.ScaledWidth(),
               SlotSprite.ScaledHeight());
}

int SaveMenu::SlotOnPage(glm::vec2 point) const {
  for (int i = 0; i < Columns * Rows; i++) {
    if (SlotRect(i).ContainsPoint(point)) return i;
  }
  return -1;
}

int SaveMenu::SelectedId() const {
  return ActivePage * Columns * Rows + Cursor;
}

void SaveMenu::Choose() {
  const int id = SelectedId();
  const uint8_t status =
      SaveSystem::GetSaveStatus(TypeFor(ActiveMenuType), id);
  // Loading needs a filled slot; saving can target any slot.
  if (status == 0 && ActiveMenuType != SaveMenuPageType::Save) return;
  // The script loads with LoadData(SW_SAVEFILETYPE, SW_SAVEFILENO).
  ScrWork[SW_SAVEFILETYPE] = (int)TypeFor(ActiveMenuType);
  ScrWork[SW_SAVEFILENO] = id;
  ScrWork[SW_SAVEFILESTATUS] = status;
  ChoiceMade = true;
}

void SaveMenu::Update(float dt) {
  const bool active = ScrWork[SW_SYSSUBMENUNO] == SubMenuNo &&
                      ScrWork[SW_SYSSUBMENUCT] > 0 && ActiveMenuType;
  if (active) {
    Show();
  } else {
    Hide();
  }
  if (State == Showing && ScrWork[SW_SYSSUBMENUCT] >= 16) State = Shown;
  if (State == Hiding && ScrWork[SW_SYSSUBMENUCT] <= 0) State = Hidden;
  if (!active) State = ScrWork[SW_SYSSUBMENUCT] > 0 ? Hiding : Hidden;
  if (State != Shown || ChoiceMade || SysMesBox::AnyVisible()) return;

  const int perPage = Columns * Rows;
  const uint32_t pressed = PADinputButtonWentDown;
  if (pressed & PAD1DOWN) Cursor = (Cursor + Columns) % perPage;
  if (pressed & PAD1UP) Cursor = (Cursor + perPage - Columns) % perPage;
  if (pressed & PAD1RIGHT) {
    if (Cursor % Columns == Columns - 1) {
      ActivePage = (ActivePage + 1) % Pages;
      Cursor -= Columns - 1;
    } else {
      Cursor++;
    }
  }
  if (pressed & PAD1LEFT) {
    if (Cursor % Columns == 0) {
      ActivePage = (ActivePage + Pages - 1) % Pages;
      Cursor += Columns - 1;
    } else {
      Cursor--;
    }
  }
  if (pressed & (PAD1R1 | PAD1R2)) ActivePage = (ActivePage + 1) % Pages;
  if (pressed & (PAD1L1 | PAD1L2)) ActivePage = (ActivePage + Pages - 1) % Pages;

  // Pointer: hover selects, a click on a slot confirms, wheel turns pages.
  const int hovered = SlotOnPage(Input::CurMousePos);
  if (hovered >= 0 && (Input::PrevMousePos != Input::CurMousePos ||
                       (PADinputMouseWentDown & PAD1A)))
    Cursor = hovered;
  if (Input::MouseWheelDeltaY > 0) ActivePage = (ActivePage + Pages - 1) % Pages;
  if (Input::MouseWheelDeltaY < 0) ActivePage = (ActivePage + 1) % Pages;

  if ((pressed & PAD1A) ||
      ((PADinputMouseWentDown & PAD1A) && hovered == Cursor && hovered >= 0)) {
    Choose();
  }
}

void SaveMenu::Render() {
  if (State == Hidden || !ActiveMenuType) return;
  const float alpha = std::clamp(ScrWork[SW_SYSSUBMENUCT] / 16.0f, 0.0f, 1.0f);
  const glm::vec4 col(1.0f, 1.0f, 1.0f, alpha);

  const Sprite& background = *ActiveMenuType == SaveMenuPageType::Save
                                 ? SaveBackground
                             : *ActiveMenuType == SaveMenuPageType::Load
                                 ? LoadBackground
                                 : QuickLoadBackground;
  Renderer->DrawSprite(background, glm::vec2(0.0f), col);

  const SaveSystem::SaveType type = TypeFor(ActiveMenuType);
  const auto drawNumber = [&](int value, glm::vec2 pos, float scale) {
    const std::string digits = fmt::format("{:02d}", value);
    for (char c : digits) {
      const Sprite& digit = DigitSprites[c - '0'];
      Renderer->DrawSprite(digit, RectF(pos.x, pos.y, digit.ScaledWidth() * scale,
                                        digit.ScaledHeight() * scale),
                           col);
      pos.x += digit.ScaledWidth() * scale;
    }
  };

  for (int slot = 0; slot < Columns * Rows; slot++) {
    const RectF rect = SlotRect(slot);
    const int id = ActivePage * Columns * Rows + slot;
    glm::vec4 plateCol = col;
    if (slot != Cursor) plateCol.r = plateCol.g = plateCol.b = 0.7f;
    Renderer->DrawSprite(SlotSprite, rect.GetPos(), plateCol);

    const RectF thumb(rect.X + ThumbnailOffset.x, rect.Y + ThumbnailOffset.y,
                      ThumbnailSize.x, ThumbnailSize.y);
    if (SaveSystem::GetSaveStatus(type, id) != 0) {
      Renderer->DrawSprite(SaveSystem::GetSaveThumbnail(type, id), thumb, col);
    } else {
      Renderer->DrawSprite(NoDataSprite, thumb, col);
    }
    drawNumber(id + 1, rect.GetPos() + SlotNumberOffset, SlotNumberScale);
    if (slot == Cursor) {
      Renderer->DrawSprite(CursorSprite,
                           glm::vec2(rect.X - CursorSprite.ScaledWidth() - 4.0f,
                                     rect.Center().y -
                                         CursorSprite.ScaledHeight() / 2.0f),
                           col);
    }
  }

  // Page "N/6"
  const Sprite& page = DigitSprites[ActivePage + 1];
  Renderer->DrawSprite(page, PagePos, col);
  Renderer->DrawSprite(PageTotalSprite,
                       PagePos + glm::vec2(page.ScaledWidth() + 2.0f,
                                           page.ScaledHeight() -
                                               PageTotalSprite.ScaledHeight()),
                       col);
}

}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
