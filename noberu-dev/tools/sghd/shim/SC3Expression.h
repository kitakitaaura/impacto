#pragma once
// Length-only SC3 expression: same token sizes as impacto's
// ExpressionParser::GetTokens (src/vm/expression.cpp).
#include <cstdint>
#include <stdexcept>
class SC3Expression {
 public:
  SC3Expression() = default;
  explicit SC3Expression(const uint8_t* p) {
    const uint8_t* s = p;
    while (*p) {
      int8_t t = (int8_t)*p;
      if (t >= 0) {
        p += 2;
      } else {
        static const int sz[4] = {1, 2, 3, 5};
        p += sz[(t & 0x60) >> 5] + 1;
      }
      if (p - s > 4096) throw std::runtime_error("runaway expression");
    }
    len_ = (uint32_t)(p - s) + 1;
  }
  uint32_t rawLength() const { return len_; }
 private:
  uint32_t len_ = 0;
};
