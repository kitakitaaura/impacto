#pragma once

#include "vfsarchive.h"
#include <string>
#include <vector>

namespace Impacto {
namespace Io {

struct TextMetaEntry;

struct FolderArchiveParameters {
  std::optional<std::string> OrderFilePath;
  std::optional<std::string> WhitelistPattern;
  // File ids by name: entry i is id i, matched against file stems ignoring
  // case. Missing files leave their id unused instead of renumbering the
  // rest (Steam STEINS;GATE: the exe's movie table lists IMV049, which the
  // game doesn't ship).
  std::optional<std::vector<std::string>> Names;
};

class FSFolderArchive : public VfsArchive {
 public:
  IoError Open(FileMeta* file, Stream** outStream) override;
  IoError GetCurrentSize(FileMeta* file, int64_t& outSize) override;

  static IoError Create(Stream* stream, VfsArchive** outArchive,
                        std::optional<FolderArchiveParameters> params);

 private:
  static IoError SortTOCLexicographically(
      FSFolderArchive* result, std::filesystem::path const& rootPath);
  static IoError SortTOCByOrderFile(FSFolderArchive* result,
                                    VfsArchive** orderLines);
  static void AssignIdsByName(FSFolderArchive* result,
                              std::vector<std::string> const& names);
  std::vector<TextMetaEntry> TOC;
};

}  // namespace Io
}  // namespace Impacto