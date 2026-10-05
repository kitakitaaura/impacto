#pragma once

#ifdef __EMSCRIPTEN__

#include "stream.h"
#include <memory>
#include <string>

namespace Impacto {
namespace Io {

// Browser builds: a read-only stream over a File the page registered under a
// path (Module.noberuFiles, see web/impacto-web.js). Game archives are
// gigabytes, so nothing is copied into the wasm heap up front; reads await a
// Blob slice through ASYNCIFY. A small block cache shared between duplicates
// keeps the many tiny header reads (MPK tables, LAY files) cheap.
class WebFileStream : public Stream {
 public:
  // IoError_NotFound when the page registered no file at this path.
  static IoError Create(std::string const& path, Stream** out);

  int64_t Read(void* buffer, int64_t sz) override;
  int64_t Seek(int64_t offset, int origin) override;
  IoError Duplicate(Stream** outStream) override;

  struct Shared;

 private:
  explicit WebFileStream(std::shared_ptr<Shared> shared);
  std::shared_ptr<Shared> Data;
};

}  // namespace Io
}  // namespace Impacto

#endif
