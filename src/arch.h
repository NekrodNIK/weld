#pragma once
#include "src/ints.h"

namespace weld::arch {
struct i386 {
  static constexpr const char* name = "i386";
  static constexpr bool is_64 = false;
  static constexpr bool is_le = true;
};
struct x86_64 {
  static constexpr const char* name = "x86_64";
  static constexpr bool is_64 = true;
  static constexpr bool is_le = true;
};

enum class Enum : i32 {
  unsupported,
  i386,
  x86_64,
};
} // namespace weld::arch
