#pragma once
#include <climits>
#include <cstdint>

namespace weld {
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// WARN: u8 is used as the primary byte type in the project.
// For reinterpret_cast, it's important that it be an unsigned char:
// https://en.cppreference.com/w/cpp/language/reinterpret_cast.html#Type_aliasing
using u8 = unsigned char;
static_assert(sizeof(unsigned char) == 1);

using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
} // namespace weld
