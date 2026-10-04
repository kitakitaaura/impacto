#include "nametagdisplay.h"

#include "../profile/dialogue.h"
#include "chlcc/nametagdisplay.h"
#include "cc/nametagdisplay.h"
#include "../log.h"

#include <numeric>

namespace Impacto {

using namespace Impacto::Profile::Dialogue;

std::unique_ptr<NametagDisplay> NametagDisplay::Create() {
  switch (NametagCurrentType) {
    default:
      ImpLog(LogLevel::Warning, LogChannel::General,
             "Attempted to create unexpected nametag type {:d}; defaulting to "
             "Void",
             static_cast<int>(NametagCurrentType));
      [[fallthrough]];
    case NametagType::None:
      return std::make_unique<VoidNametagDisplay>();

    case NametagType::Sprite:
      return std::make_unique<SpriteNametagDisplay>();

    case NametagType::TwoPiece:
    case NametagType::ThreePiece:
      return std::make_unique<ThreePieceNametagDisplay>();

    case NametagType::CHLCC:
      return std::make_unique<CHLCC::NametagDisplay>();

    case NametagType::CC:
      return std::make_unique<CC::NametagDisplay>();
  }
}

void VoidNametagDisplay::Render(const NameInfo& nameInfo,
                                const glm::vec4 tint) {
  if (Hidden || !nameInfo.RenderWindow || nameInfo.Name.empty()) return;

  DialogueFont->DrawProcessedText(nameInfo.Name, tint.a,
                                  RendererOutlineMode::Full);
}

void SpriteNametagDisplay::Render(const NameInfo& nameInfo,
                                  const glm::vec4 tint) {
  if (Hidden || !nameInfo.RenderWindow || nameInfo.Name.empty()) return;

  Renderer->DrawSprite(NametagSprite, NametagPosition, tint);

  DialogueFont->DrawProcessedText(nameInfo.Name, tint.a,
                                  RendererOutlineMode::Full);
}

void ThreePieceNametagDisplay::Render(const NameInfo& nameInfo,
                                      const glm::vec4 tint) {
  if (Hidden || !nameInfo.RenderWindow || nameInfo.Name.empty()) return;

  const float textWidth = GetTextWidth(nameInfo.Name);
  glm::vec2 position = NametagPosition;
  if (NametagCenterX) {
    position.x =
        *NametagCenterX - textWidth / 2.0f - NametagLeftSprite.ScaledWidth();
  }

  Renderer->DrawSprite(NametagLeftSprite, position, tint);

  float rightX = position.x + NametagLeftSprite.ScaledWidth() + textWidth;

  if (NametagCurrentType == NametagType::ThreePiece) {
    rightX -= NametagMiddleBaseWidth;

    // Draw middle sprites
    const float middleBegin = position.x + NametagLeftSprite.ScaledWidth();

    Sprite middleSprite = NametagMiddleSprite;
    for (float x = middleBegin; x < rightX;
         x += NametagMiddleSprite.ScaledWidth()) {
      middleSprite.SetScaledWidth(
          std::min(rightX - x, NametagMiddleSprite.ScaledWidth()));
      Renderer->DrawSprite(middleSprite, {x, position.y}, tint);
    }
  }

  Renderer->DrawSprite(NametagRightSprite, {rightX, position.y}, tint);

  DialogueFont->DrawProcessedText(nameInfo.Name, tint.a,
                                  RendererOutlineMode::Full);
}

}  // namespace Impacto
