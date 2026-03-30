#pragma once
#include "ints.h"
#include <filesystem>
#include <optional>
#include <span>

namespace weld {
using std::span;

class MappedFile {
  u8* ptr_;
  size_t size_;
  bool map(const char* path);
  void unmap();
  MappedFile() : ptr_(nullptr), size_(0) {};

public:
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& src);
  MappedFile& operator=(MappedFile&& src);
  ~MappedFile();
  static std::optional<MappedFile> open(const std::filesystem::path& path);
  span<uint8_t> data();
};
} // namespace weld
