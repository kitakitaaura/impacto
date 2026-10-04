#include "titlemenu.h"

#include "../../profile/ui/titlemenu.h"
#include "../../profile/games/sghd/titlemenu.h"
#include "../../profile/scriptvars.h"
#include "../../renderer/renderer.h"
#include "../../vm/interface/input.h"
#include "../../mem.h"

#include <SDL3/SDL_stdinc.h>

namespace Impacto {
namespace UI {
namespace SGHD {

using namespace Impacto::Profile::TitleMenu;
using namespace Impacto::Profile::SGHD::TitleMenu;
using namespace Impacto::Profile::ScriptVars;
using namespace Impacto::Vm::Interface;

void TitleMenu::Show() {
  if (State == Hidden || State == Hiding) {
    FadeAnimation.DurationIn = FadeAnimation.DurationOut = FadeDuration;
    FadeAnimation.StartIn();
    PressStartAnimation.DurationIn = PressToStartAnimDurationIn;
    PressStartAnimation.DurationOut = PressToStartAnimDurationOut;
    PressStartAnimation.LoopMode = AnimationLoopMode::ReverseDirection;
    PressStartAnimation.StartIn();
    State = Showing;
  }
}

void TitleMenu::Hide() {
  if (State == Shown || State == Showing) {
    FadeAnimation.StartOut();
    State = Hiding;
  }
}

bool TitleMenu::Pressed(uint32_t pad) const {
  // Dev aid: IMPACTO_TITLE_AUTOSTART=1 confirms Press Enter and START.
  static const bool autostart = SDL_getenv("IMPACTO_TITLE_AUTOSTART");
  if (autostart && pad == PAD1A && MainCursor == 0) return true;
  return (PADinputButtonWentDown | PADinputMouseWentDown) & pad;
}

void TitleMenu::Choose(int id) {
  ScrWork[SW_TITLECUR] = id;
  SetFlag(SF_TITLEEND, true);
}

void TitleMenu::ScriptTick(int mode) {
  Ticked = true;
  if (mode == 0) {
    CurStage = Stage::PressStart;
    return;
  }
  if (mode != 1 || State != Shown || GetFlag(SF_TITLEEND)) return;

  const int mainCount = (int)MainItems.size();
  switch (CurStage) {
    case Stage::PressStart:
      if (Pressed(PAD1A) || Pressed(PAD1START)) {
        CurStage = Stage::Main;
        SetFlag(SF_TITLEEND, true);
      }
      break;

    case Stage::Main: {
      if (Pressed(PAD1DOWN)) MainCursor = (MainCursor + 1) % mainCount;
      if (Pressed(PAD1UP)) MainCursor = (MainCursor + mainCount - 1) % mainCount;
      const auto& item = MainItems[MainCursor];
      if (Pressed(PAD1A)) {
        if (item.SubItems.empty()) {
          Choose(item.Id);
        } else {
          SubCursor = 0;
          CurStage = Stage::Sub;
        }
      }
    } break;

    case Stage::Sub: {
      const auto& subs = MainItems[MainCursor].SubItems;
      const int subCount = (int)subs.size();
      if (Pressed(PAD1DOWN)) SubCursor = (SubCursor + 1) % subCount;
      if (Pressed(PAD1UP)) SubCursor = (SubCursor + subCount - 1) % subCount;
      if (Pressed(PAD1A)) Choose(subs[SubCursor].Id);
      if (Pressed(PAD1B)) CurStage = Stage::Main;
    } break;
  }
}

void TitleMenu::Update(float dt) {
  if (GetFlag(SF_TITLEMODE)) {
    Show();
  } else {
    Hide();
  }
  FadeAnimation.Update(dt);
  PressStartAnimation.Update(dt);
  if (State == Showing && FadeAnimation.IsIn()) State = Shown;
  if (State == Hiding && FadeAnimation.IsOut()) State = Hidden;
}

void TitleMenu::Render() {
  if (State == Hidden) return;
  const float alpha = FadeAnimation.Progress;
  const glm::vec4 col(1.0f, 1.0f, 1.0f, alpha);

  Renderer->DrawSprite(BackgroundSprite, BackgroundPos, col);

  if (CurStage == Stage::PressStart) {
    glm::vec4 blink = col;
    blink.a *= glm::mix(0.25f, 1.0f, PressStartAnimation.Progress);
    Renderer->DrawSprite(PressToStartSprite, PressToStartPos, blink);
    return;
  }

  for (int i = 0; i < (int)MainItems.size(); i++) {
    const glm::vec2 pos = MainItemPos + glm::vec2(0.0f, i * MainItemSpacing);
    const bool selected = i == MainCursor;
    Renderer->DrawSprite(
        selected ? MainItems[i].Highlighted : MainItems[i].Normal, pos, col);

    if (selected && CurStage == Stage::Sub) {
      const auto& subs = MainItems[i].SubItems;
      for (int j = 0; j < (int)subs.size(); j++) {
        const glm::vec2 subPos =
            pos + SubItemOffset + glm::vec2(0.0f, j * SubItemSpacing);
        const bool subSelected = j == SubCursor;
        Renderer->DrawSprite(subSelected ? subs[j].Highlighted : subs[j].Normal,
                             subPos, col);
        if (subSelected) {
          Renderer->DrawSprite(ArrowSprite, subPos + ArrowOffset, col);
        }
      }
    }
  }
}

}  // namespace SGHD
}  // namespace UI
}  // namespace Impacto
