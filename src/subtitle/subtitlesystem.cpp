#include <fmt/format.h>
#include "subtitlesystem.h"

#ifndef IMPACTO_DISABLE_LIBASS
#include "ass/subtitlerenderer.h"
#endif

#include "../util.h"
#include "../profile/game.h"
#include "../profile/profile.h"
#include "../userconfig.h"
#include "../profile/subtitle.h"
#include "../log.h"
#include "../io/physicalfilestream.h"

namespace Impacto::Subtitle {
using namespace Profile::Subtitle;

static void SubstituteFonts(std::vector<char>& script);

void SubtitleInit() {
  switch (UserConfig::AdvancedSettings.SubtitleAssBackend) {
#ifndef IMPACTO_DISABLE_LIBASS
    case SubtitleAssBackendType::LibAss:
      Ass::SubtitleRenderer::InitSystem();
      break;
#endif
    case SubtitleAssBackendType::None:
      break;
    default:
      ImpLog(LogLevel::Warning, LogChannel::Subtitle,
             "Unknown or unsupported ass subtitle backend selected!.\n");
  }
  switch (UserConfig::AdvancedSettings.SubtitleTextBackend) {
    case SubtitleTextBackendType::None:
      break;
    default:
      ImpLog(LogLevel::Warning, LogChannel::Subtitle,
             "Unknown or unsupported text subtitle backend selected!.\n");
  }
  switch (UserConfig::AdvancedSettings.SubtitleBmpBackend) {
    case SubtitleBmpBackendType::None:
      break;
    default:
      ImpLog(LogLevel::Warning, LogChannel::Subtitle,
             "Unknown or unsupported bitmap subtitle backend selected!.\n");
  }
}

SubtitlePlayer::SubtitlePlayer(float width, float height) {
  switch (UserConfig::AdvancedSettings.SubtitleAssBackend) {
#ifndef IMPACTO_DISABLE_LIBASS
    case SubtitleAssBackendType::LibAss:
      Backends[GetBackendIndex(SubtitleType::Ass)] =
          std::make_unique<Ass::SubtitleRenderer>(*this, width, height);
      break;
#endif
    case SubtitleAssBackendType::None:
    default:  // backend compiled out
      break;
  }
  switch (UserConfig::AdvancedSettings.SubtitleTextBackend) {
    case SubtitleTextBackendType::None:
      break;
  }
  switch (UserConfig::AdvancedSettings.SubtitleBmpBackend) {
    case SubtitleBmpBackendType::None:
      break;
  }
}

bool SubtitlePlayer::CanAddTrack(
    int trackId, SubtitleType type,
    Profile::Subtitle::SubtitleConfigType config) const {
  using UserConfig::EnhancementsSettings;
  const auto& backend = Backends[GetBackendIndex(type)];
  if (!backend) {
    ImpLog(LogLevel::Warning, LogChannel::Subtitle,
           "Subtitle backend not initialized for {}", type);
    return false;
  }
  if ((+config & +EnhancementsSettings.SubtitleConfig) == 0) {
    ImpLog(LogLevel::Info, LogChannel::Subtitle,
           "Current subtitle mode is {}, skipping track {}, which is mode {}",
           EnhancementsSettings.SubtitleConfig, trackId, config);
    return false;
  }
  return true;
}

void SubtitlePlayer::AddTrackFile(
    int trackId, SubtitleType type, std::string const& path,
    Profile::Subtitle::SubtitleConfigType config) {
  if (!CanAddTrack(trackId, type, config)) return;
  auto& backend = Backends[GetBackendIndex(type)];
  auto data = ReadWholeFile(path);
  if (!data) {
    ImpLog(LogLevel::Warning, LogChannel::Subtitle,
           "Failed to read subtitle file {}", path);
    return;
  }
  if (type == SubtitleType::Ass) SubstituteFonts(*data);
  backend->AddTrackFile(trackId, *data);
}

// Profile FontSubstitutes in an ASS script: style font fields
// (",Name," and the vertical ",@Name,") and \fn override tags.
static void SubstituteFonts(std::vector<char>& script) {
  if (SubtitleFontSubstitutes.empty()) return;
  std::string text(script.begin(), script.end());
  const auto replaceAll = [&text](std::string const& from,
                                  std::string const& to) {
    for (size_t pos = text.find(from); pos != std::string::npos;
         pos = text.find(from, pos + to.size())) {
      text.replace(pos, from.size(), to);
    }
  };
  for (const auto& [from, to] : SubtitleFontSubstitutes) {
    for (const char* prefix : {",", ",@", "\\fn", "\\fn@"}) {
      const char* suffix = prefix[0] == ',' ? "," : "";
      replaceAll(prefix + from + suffix, prefix + to + suffix);
    }
  }
  script.assign(text.begin(), text.end());
}

std::optional<std::vector<char>> ReadWholeFile(std::string const& path) {
  Io::Stream* stream{};
  if (Io::OpenLooseFile(path, &stream) != IoError_OK) return std::nullopt;
  std::vector<char> data(stream->Meta.Size);
  const int64_t read = stream->Read(data.data(), data.size());
  delete stream;
  if (read != (int64_t)data.size()) return std::nullopt;
  return data;
}

void SubtitlePlayer::PushEntry(int trackId, SubtitleEntry entry) {
  auto trackTypeItr = TrackTypeMap.find(trackId);
  if (trackTypeItr == TrackTypeMap.end()) {
    ImpLog(LogLevel::Warning, LogChannel::Subtitle,
           "Track {} not found in Subtitle Track Map", trackId);
    return;
  }
  auto type = trackTypeItr->second;
  const auto backendIndex = GetBackendIndex(type);
  auto& backend = Backends[backendIndex];
  if (!backend) {
    ImpLog(LogLevel::Warning, LogChannel::Subtitle,
           "Subtitle backend not initialized for {}", type);
    return;
  }
  backend->AddSubtitleEntry(trackId, std::move(entry));
}

void SubtitlePlayer::UpdateElapsedTime(Video::Clock::Microseconds elapsedTime) {
  for (auto& backend : Backends) {
    if (backend) {
      backend->Update(elapsedTime);
    };
  }
}

void SubtitlePlayer::Render() {
  for (auto& backend : Backends) {
    if (backend) {
      backend->Render();
    };
  }
}

int8_t SubtitlePlayer::GetBackendIndex(SubtitleType type) const {
  switch (type) {
    case SubtitleType::Ass:
      return 0;
    case SubtitleType::Bitmap:
      return 1;
    case SubtitleType::Text:
      return 2;
    case SubtitleType::None:
      break;
  }
  throw std::runtime_error(fmt::format("Invalid SubtitleType {}", type));
}

}  // namespace Impacto::Subtitle