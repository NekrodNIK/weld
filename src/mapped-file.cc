#include "weld.h"
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
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
    file.filename_ = path.filename();
    return file;
  } else {
    return {};
  }
}

MappedFile::~MappedFile() { unmap(); }
std::string_view MappedFile::filename() const { return filename_; }
std::span<const u8> MappedFile::data() const { return std::span(ptr_, size_); }
const u8* MappedFile::raw() const { return ptr_; }
size_t MappedFile::size() const { return size_; }
std::span<u8> MappedFile::data() { return std::span(ptr_, size_); }
u8* MappedFile::raw() { return ptr_; }

} // namespace weld
