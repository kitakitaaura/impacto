#include "titlemenu.h"

#include "../../profile/ui/titlemenu.h"
#include "../../profile/games/sghd/titlemenu.h"
#include "../../profile/scriptvars.h"
#include "../../renderer/renderer.h"
#include "../../vm/interface/input.h"
#include "../../mem.h"
#include "../../log.h"
#include "../../inputsystem.h"

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
  return (PADinputButtonWentDown | PADinputMouseWentDown) & pad;
}

RectF TitleMenu::MainItemRect(int i) const {
  const Sprite& sprite = MainItems[i].Normal;
  const glm::vec2 pos = MainItemPos + glm::vec2(0.0f, i * MainItemSpacing);
  return RectF(pos.x, pos.y, sprite.ScaledWidth(), sprite.ScaledHeight());
}

RectF TitleMenu::SubItemRect(int j) const {
  const Sprite& sprite = MainItems[MainCursor].SubItems[j].Normal;
  const glm::vec2 pos = MainItemPos +
                        glm::vec2(0.0f, MainCursor * MainItemSpacing) +
                        SubItemOffset + glm::vec2(0.0f, j * SubItemSpacing);
  return RectF(pos.x, pos.y, sprite.ScaledWidth(), sprite.ScaledHeight());
}

bool TitleMenu::PointerClick(bool overItem) const {
  return (PADinputMouseWentDown & PAD1A) && overItem;
}

void TitleMenu::Choose(int id) {
  ScrWork[SW_TITLECUR] = id;
  SetFlag(SF_TITLEEND, true);
}

void TitleMenu::ScriptTick(int mode) {
  Ticked = true;
  // Dev aid: IMPACTO_TITLE_AUTOSTART=1 passes Press Enter and picks START;
  // any other value is a title item id to pick (e.g. 11 = LOAD, 10 = QUICKLOAD).
  static bool devPickRead = false;
  if (!devPickRead) {
    devPickRead = true;
    if (const char* pick = SDL_getenv("IMPACTO_TITLE_AUTOSTART")) {
      const int id = std::atoi(pick);
      DevPick = id == 1 ? 0 : id;
    }
  }
  if (mode == 0) {
    CurStage = Stage::PressStart;
    return;
  }
  if (mode != 1 || State != Shown || GetFlag(SF_TITLEEND)) return;

  const int mainCount = (int)MainItems.size();
  switch (CurStage) {
    case Stage::PressStart:
      if (Pressed(PAD1A) || Pressed(PAD1START) || DevPick) {
        CurStage = Stage::Main;
        SetFlag(SF_TITLEEND, true);
      }
      break;

    case Stage::Main: {
      if (DevPick) {
        Choose(*DevPick);
        DevPick.reset();
        break;
      }
      if (Pressed(PAD1DOWN)) MainCursor = (MainCursor + 1) % mainCount;
      if (Pressed(PAD1UP)) MainCursor = (MainCursor + mainCount - 1) % mainCount;
      bool overItem = false;
      for (int i = 0; i < mainCount; i++) {
        if (MainItemRect(i).ContainsPoint(Input::CurMousePos)) {
          // A click picks the item under the pointer even if no move came
          // first (touch taps, or a press and release within one frame).
          if (Input::PrevMousePos != Input::CurMousePos ||
              (PADinputMouseWentDown & PAD1A))
            MainCursor = i;
          overItem = i == MainCursor;
        }
      }
      const auto& item = MainItems[MainCursor];
      const bool confirm = (PADinputButtonWentDown & PAD1A) ||
                           PointerClick(overItem) ||
                           (Pressed(PAD1A) && !(PADinputMouseWentDown & PAD1A));
      if (confirm) {
        if (item.SubItems.empty()) {
          Choose(item.Id);
        } else {
          SubCursor = 0;
          CurStage = Stage::Sub;
          SubJustOpened = true;
        }
      }
    } break;

    case Stage::Sub: {
      if (SubJustOpened) {
        SubJustOpened = PADinputMouseIsDown & PAD1A;  // wait for release
        break;
      }
      const auto& subs = MainItems[MainCursor].SubItems;
      const int subCount = (int)subs.size();
      if (Pressed(PAD1DOWN)) SubCursor = (SubCursor + 1) % subCount;
      if (Pressed(PAD1UP)) SubCursor = (SubCursor + subCount - 1) % subCount;
      bool overItem = false;
      for (int j = 0; j < subCount; j++) {
        if (SubItemRect(j).ContainsPoint(Input::CurMousePos)) {
          // A click picks the item under the pointer even if no move came
          // first (touch taps, or a press and release within one frame).
          if (Input::PrevMousePos != Input::CurMousePos ||
              (PADinputMouseWentDown & PAD1A))
            SubCursor = j;
          overItem = j == SubCursor;
        }
      }
      if ((PADinputButtonWentDown & PAD1A) || PointerClick(overItem)) {
        Choose(subs[SubCursor].Id);
      } else if (Pressed(PAD1B) || (PADinputMouseWentDown & PAD1A)) {
        // Back out: cancel button, right click, or a click off the submenu.
        CurStage = Stage::Main;
      }
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
