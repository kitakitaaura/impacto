#pragma once

#include "../../ui/menu.h"

namespace Impacto {
namespace UI {
namespace SGHD {

// STEINS;GATE Steam in-game menu (BACK LOG ... EXIT GAME). The script runs
// SystemMenu(0) once, fades SW_SYSMENUCT to 32, then calls SystemMenu(1) every
// frame and on confirm jumps through a table indexed by SW_SYSMENUCNO. This
// menu keeps SW_SYSMENUCNO on the highlighted item; a click raises ChoiceMade
// (InstSystemMenu turns that into a confirm press).
class SystemMenu : public Menu {
 public:
  void Init() override;
  void Show() override {}
  void Hide() override {}
  void Update(float dt) override;
  void Render() override;

 private:
  RectF ItemRect(int i) const;
  int Cursor = 0;
};

}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
