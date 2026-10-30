#pragma once
#include <ankerl/unordered_dense.h>
#include <string>
#include <magic_enum/magic_enum.hpp>
#include "game.h"

namespace Impacto::Profile::Subtitle {

enum class SubtitleType : uint8_t {
  None = 0,
  Bitmap,
  Text,
  Ass,
};

enum class SubtitleConfigType : uint8_t {
  None = 0,
  Karaoke = 1 << 0,
  Translation = 1 << 1,
  All = Karaoke | Translation,
};

constexpr SubtitleConfigType operator~(SubtitleConfigType config) {
  return static_cast<SubtitleConfigType>(~to_underlying(config));
}
constexpr SubtitleConfigType operator|(SubtitleConfigType config,
                                       SubtitleConfigType other) {
  return static_cast<SubtitleConfigType>(to_underlying(config) |
                                         to_underlying(other));
}
constexpr SubtitleConfigType operator&(SubtitleConfigType config,
                                       SubtitleConfigType other) {
  return static_cast<SubtitleConfigType>(to_underlying(config) &
                                         to_underlying(other));
}
constexpr SubtitleConfigType operator^(SubtitleConfigType config,
                                       SubtitleConfigType other) {
  return static_cast<SubtitleConfigType>(to_underlying(config) ^
                                         to_underlying(other));
}
constexpr SubtitleConfigType& operator|=(SubtitleConfigType& config,
                                         SubtitleConfigType other) {
  config = config | other;
  return config;
}
constexpr SubtitleConfigType& operator&=(SubtitleConfigType& config,
                                         SubtitleConfigType other) {
  config = config & other;
  return config;
}
constexpr SubtitleConfigType& operator^=(SubtitleConfigType& config,
                                         SubtitleConfigType other) {
  config = config ^ other;
  return config;
}

struct SubtitleTrackFile {
  SubtitleType Type;
  SubtitleConfigType Config;
  std::optional<int> Id;
  std::optional<std::string> Path;
};

using SubtitleTrackFiles = std::vector<SubtitleTrackFile>;
using SubtitleMountMapping =
    ankerl::unordered_dense::map<std::variant<uint32_t, std::string>,
                                 SubtitleTrackFiles>;
inline ankerl::unordered_dense::map<std::string, SubtitleMountMapping>
    SubtitleMappings;
inline std::vector<std::string> SubtitleFontsDir;
// Family libass falls back to when a style's font is missing (browser builds
// have no system fonts at all).
inline std::string SubtitleDefaultFont = "sans-serif";
// Font families swapped in ASS scripts before loading (missing -> available),
// for fonts a script names but nothing ships; the default family above may
// lack the glyphs (CJK lyrics).
inline ankerl::unordered_dense::map<std::string, std::string>
    SubtitleFontSubstitutes;

void Configure();
}  // namespace Impacto::Profile::Subtitle

template <>
struct magic_enum::customize::enum_range<
    Impacto::Profile::Subtitle::SubtitleConfigType> {
  static constexpr bool is_flags = true;
};