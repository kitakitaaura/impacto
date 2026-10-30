#pragma once

#include <cstdint>
#include <string>
#include <ankerl/unordered_dense.h>

namespace Impacto {
namespace Profile {
namespace Vfs {
// Replacement audio tracks for movies: mount -> movie id -> loose file
// played instead of the movie's own audio.
inline ankerl::unordered_dense::map<
    std::string, ankerl::unordered_dense::map<uint32_t, std::string>>
    MovieAudio;

void Configure();
}
}  // namespace Profile
}  // namespace Impacto