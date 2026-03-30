#include "weld.h"
#include <filesystem>
#include <optional>
#include <utility>

namespace weld {
namespace fs = ::std::filesystem;

MappedFile::MappedFile(MappedFile&& src) {
  ptr_ = std::exchange(src.ptr_, nullptr);
  size_ = std::exchange(src.size_, 0);
}

MappedFile& MappedFile::operator=(MappedFile&& src) {
  ptr_ = std::exchange(src.ptr_, nullptr);
  size_ = std::exchange(src.size_, 0);
  return *this;
};

std::optional<MappedFile> MappedFile::open(const fs::path& path) {
  if (!fs::exists(path) || fs::is_directory(path)) {
    return {};
  }
  MappedFile file;
  if (file.map(path.c_str())) {
    return file;
  } else {
    return {};
  }
}

span<uint8_t> MappedFile::data() { return std::span(ptr_, size_); }
MappedFile::~MappedFile() { unmap(); }
} // namespace weld
