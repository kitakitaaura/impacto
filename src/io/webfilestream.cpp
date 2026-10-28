#ifdef __EMSCRIPTEN__

#include "webfilestream.h"
#include "../log.h"

#include "../impacto.h"

#include <emscripten.h>
#include <emscripten/threading.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <list>
#include <mutex>
#include <vector>

// The page keeps File objects in Module.noberuFiles (a Map from path to File).
EM_JS(int, noberu_file_open, (const char* path), {
  const file = Module.noberuFiles && Module.noberuFiles.get(UTF8ToString(path));
  if (!file) return -1;
  Module.noberuOpenFiles = Module.noberuOpenFiles || [];
  Module.noberuOpenFiles.push(file);
  return Module.noberuOpenFiles.length - 1;
});

EM_JS(double, noberu_file_size, (int handle), {
  return Module.noberuOpenFiles[handle].size;
});

// A URL worker threads can read the file through (threaded builds): the dev
// server's for ?local=1 files, else a blob: URL for the picked File.
EM_JS(char*, noberu_file_url, (int handle), {
  const file = Module.noberuOpenFiles[handle];
  const url = file.url || (file.noberuUrl = file.noberuUrl || URL.createObjectURL(file));
  return stringToNewUTF8(url);
});

// Worker threads can't await; a synchronous ranged XHR works there (blob: and
// http URLs both answer 206 with the slice).
EM_JS(int, noberu_url_read_sync,
      (const char* url, double offset, void* buffer, int length), {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', UTF8ToString(url), false);
  xhr.responseType = 'arraybuffer';
  xhr.setRequestHeader('Range', 'bytes=' + offset + '-' + (offset + length - 1));
  xhr.send();
  if (xhr.status !== 206 && xhr.status !== 200) return -1;
  const data = new Uint8Array(xhr.response);
  HEAPU8.set(data.subarray(0, length), buffer);
  return Math.min(data.byteLength, length);
});

EM_ASYNC_JS(int, noberu_file_read,
            (int handle, double offset, void* buffer, int length), {
  const file = Module.noberuOpenFiles[handle];
  const data = await file.slice(offset, offset + length).arrayBuffer();
  HEAPU8.set(new Uint8Array(data), buffer);
  return data.byteLength;
});

namespace Impacto {
namespace Io {

namespace {
constexpr int64_t BlockSize = 256 * 1024;
constexpr size_t MaxCachedBlocks = 16;
}  // namespace

struct WebFileStream::Shared {
  int Handle = -1;
  int64_t Size = 0;
  std::string Url;  // for reads off the main thread
  // Most recently used first. Blocks are shared_ptrs so a reader keeps its
  // block while another thread evicts it.
  std::list<std::pair<int64_t, std::shared_ptr<std::vector<uint8_t>>>> Blocks;
  std::mutex Lock;

  int ReadAt(int64_t offset, void* dest, int length) {
#if IMPACTO_HAVE_THREADS
    if (!emscripten_is_main_runtime_thread())
      return noberu_url_read_sync(Url.c_str(), (double)offset, dest, length);
#endif
    return noberu_file_read(Handle, (double)offset, dest, length);
  }

  std::shared_ptr<std::vector<uint8_t>> GetBlock(int64_t index) {
    {
      std::lock_guard lock(Lock);
      for (auto it = Blocks.begin(); it != Blocks.end(); ++it) {
        if (it->first == index) {
          Blocks.splice(Blocks.begin(), Blocks, it);
          return Blocks.front().second;
        }
      }
    }
    // Read without the lock: the main thread awaits (ASYNCIFY) here.
    const int64_t start = index * BlockSize;
    const int64_t length = std::min(BlockSize, Size - start);
    auto data = std::make_shared<std::vector<uint8_t>>(length);
    if (ReadAt(start, data->data(), (int)length) != length) return nullptr;
    std::lock_guard lock(Lock);
    Blocks.emplace_front(index, data);
    if (Blocks.size() > MaxCachedBlocks) Blocks.pop_back();
    return data;
  }
};

WebFileStream::WebFileStream(std::shared_ptr<Shared> shared)
    : Data(std::move(shared)) {
  Meta.Size = Data->Size;
}

IoError WebFileStream::Create(std::string const& path, Stream** out) {
  const int handle = noberu_file_open(path.c_str());
  if (handle < 0) return IoError_NotFound;
  auto shared = std::make_shared<Shared>();
  shared->Handle = handle;
  shared->Size = (int64_t)noberu_file_size(handle);
#if IMPACTO_HAVE_THREADS
  char* url = noberu_file_url(handle);
  shared->Url = url;
  free(url);
#endif
  auto* stream = new WebFileStream(std::move(shared));
  stream->Meta.FileName = path;
  *out = stream;
  return IoError_OK;
}

int64_t WebFileStream::Read(void* buffer, int64_t sz) {
  if (sz < 0) return IoError_Fail;
  if (Position >= Meta.Size) return IoError_Eof;
  sz = std::min(sz, Meta.Size - Position);
  auto* dest = static_cast<uint8_t*>(buffer);

  // Big reads (whole images, voice clips) go straight to the file.
  if (sz >= BlockSize) {
    const int read = Data->ReadAt(Position, dest, (int)sz);
    if (read <= 0) return IoError_Fail;
    Position += read;
    return read;
  }

  int64_t done = 0;
  while (done < sz) {
    const int64_t index = (Position + done) / BlockSize;
    const auto block = Data->GetBlock(index);
    if (!block) {
      ImpLog(LogLevel::Error, LogChannel::IO, "Read failed for \"{:s}\"\n",
             Meta.FileName);
      break;
    }
    const int64_t inBlock = (Position + done) - index * BlockSize;
    const int64_t chunk =
        std::min<int64_t>(sz - done, (int64_t)block->size() - inBlock);
    std::memcpy(dest + done, block->data() + inBlock, chunk);
    done += chunk;
  }
  Position += done;
  return done > 0 ? done : IoError_Fail;
}

int64_t WebFileStream::Seek(int64_t offset, int origin) {
  int64_t target;
  switch (origin) {
    case IoSeek::Set:
      target = offset;
      break;
    case IoSeek::Cur:
      target = Position + offset;
      break;
    case IoSeek::End:
      target = Meta.Size - offset;
      break;
    default:
      return IoError_Fail;
  }
  if (target < 0) return IoError_Fail;
  Position = target;
  return Position;
}

IoError WebFileStream::Duplicate(Stream** outStream) {
  auto* copy = new WebFileStream(Data);
  copy->Meta = Meta;
  copy->Position = Position;
  *outStream = copy;
  return IoError_OK;
}

}  // namespace Io
}  // namespace Impacto

#endif
