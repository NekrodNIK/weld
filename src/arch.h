#pragma once
#include "ints.h"
#include "src/errors.h"
#include <format>
#include <ostream>
#include <type_traits>

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

template <typename E>
Enum get_enum() {
  if constexpr (std::is_same<E, i386>()) {
    return Enum::i386;
  } else if constexpr (std::is_same<E, x86_64>()) {
    return Enum::x86_64;
  }
}

std::ostream& operator<<(std::ostream& out, Enum arch);
} // namespace weld::arch
template <>
struct std::formatter<weld::arch::Enum>
    : weld::ostream_formatter<weld::arch::Enum> {};
