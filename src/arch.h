#pragma once
#include "ints.h"
#include "errors.h"
#include <format>
#include <ostream>
#include <tuple>
#include <type_traits>

namespace weld::arch {
enum class Tag : i32 {
  unsupported,
  i386,
  x86_64,
};

struct i386 {
  static constexpr const char* name = "i386";
  static constexpr Tag tag = Tag::i386;
  static constexpr bool is_64 = false;
  static constexpr bool is_le = true;
  static constexpr bool is_rela = false;
};
struct x86_64 {
  static constexpr const char* name = "x86_64";
  static constexpr Tag tag = Tag::x86_64;
  static constexpr bool is_64 = true;
  static constexpr bool is_le = true;
  static constexpr bool is_rela = true;
};

using Archs = std::tuple<i386, x86_64>;

std::ostream& operator<<(std::ostream& out, Tag arch);
} // namespace weld::arch
template <>
struct std::formatter<weld::arch::Tag>
    : weld::ostream_formatter<weld::arch::Tag> {};
