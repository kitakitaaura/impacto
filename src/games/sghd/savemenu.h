#pragma once

#include "../../ui/savemenu.h"
#include "../../animation.h"

namespace Impacto {
namespace UI {
namespace SGHD {

// STEINS;GATE Steam save/load/quick-load screen: 6 pages of 2x4 slots.
// Shown while the script's sub menu is the one that opened it (the script sets
// SW_SYSSUBMENUNO before SaveMenu(0): 3 save, 4 load) and faded by
// SW_SYSSUBMENUCT (0..16). The script polls SaveMenu(1) every
// frame and checks confirm/cancel keys itself; a choice writes SW_SAVEFILENO
// and SW_SAVEFILESTATUS and raises ChoiceMade (InstSaveMenu injects confirm).
class SaveMenu : public UI::SaveMenu {
 public:
  void Init() override;
  void Show() override;
  void Hide() override;
  void Update(float dt) override;
  void Render() override;

 private:
  int SlotOnPage(glm::vec2 point) const;
  RectF SlotRect(int slot) const;
  int SelectedId() const;
  void Choose();

  int Cursor = 0;  // slot on the current page
  int SubMenuNo = -1;
};

}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
