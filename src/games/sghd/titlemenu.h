#pragma once

#include <optional>

#include "../../ui/menu.h"
#include "../../animation.h"

namespace Impacto {
namespace UI {
namespace SGHD {

// STEINS;GATE Steam title screen. The script drives it through
// TitleMenu(mode) (10:34): mode 0 resets to "Press Enter", mode 1 is called
// every frame while the script waits on SF_TITLEEND. On a choice the menu
// writes SW_TITLECUR (main/sub item id) and raises SF_TITLEEND.
class TitleMenu : public Menu {
 public:
  void Show() override;
  void Hide() override;
  void Update(float dt) override;
  void Render() override;

  void ScriptTick(int mode);

 private:
  enum class Stage { PressStart, Main, Sub };

  void Choose(int id);
  bool Pressed(uint32_t pad) const;
  RectF MainItemRect(int i) const;
  RectF SubItemRect(int j) const;
  // Pointer: hover moves the cursor; a click must land on an item.
  bool PointerClick(bool overItem) const;

  Stage CurStage = Stage::PressStart;
  int MainCursor = 0;
  int SubCursor = 0;
  bool Ticked = false;
  // The click that opens a submenu must not also count as a click outside it.
  bool SubJustOpened = false;
  std::optional<int> DevPick;

  Animation FadeAnimation;
  Animation PressStartAnimation;
};

}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
