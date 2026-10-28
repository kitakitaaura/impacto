#pragma once

#include "../impacto.h"

namespace Impacto {

class YUVFrame {
 public:
  float Width;
  float Height;
  uint32_t LumaId;
  uint32_t CbId;
  uint32_t CrId;
  // Optional full-resolution alpha plane (YUVA420P video, e.g. Bink 2 with
  // alpha). Renderers without support leave HasAlphaPlane false.
  uint32_t AlphaId = 0;
  bool HasAlphaPlane = false;

  virtual void Init(float width, float height) = 0;

  virtual void Submit(const void* luma, const void* cb, const void* cr) = 0;
  virtual void EnableAlphaPlane() {}
  virtual void SubmitAlpha(const void* alpha) {}
  virtual void Release() = 0;
};

}  // namespace Impacto