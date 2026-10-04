#pragma once

#include "../game.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Impacto {
namespace Profile {
namespace Game {
inline uint32_t LayerCount;
inline GameFeature GameFeatures;

inline std::string WindowName;
inline std::optional<std::string> WindowIconPath;
inline std::optional<std::string> CursorArrowPath;
inline std::optional<std::string> CursorPointerPath;

inline bool LayFileBigEndian;
inline bool CharaIsMvl;

inline size_t ScreenCaptureCount = 0;
inline bool UseMoviePriority = false;
inline bool UseWaveEffects = false;

inline float LayFileTexXMultiplier;
inline float LayFileTexYMultiplier;

// The design coordinate system is: x,y from 0,0 to width,height,
// origin is top left
inline float DesignWidth;
inline float DesignHeight;

// Texture pixels per design unit for backgrounds and characters. A port whose
// art was redrawn at a higher resolution than its script coordinates (Steam
// STEINS;GATE: 1920x1080 art, 1280x720 scripts) sets this above 1.
inline float AssetScale = 1.0f;

inline int PlatformId = 0;

inline bool HasInit = false;

void Configure();
}  // namespace Game
}  // namespace Profile
}  // namespace Impacto