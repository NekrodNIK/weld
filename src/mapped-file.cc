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
  filename_ = std::move(src.filename_);
  owns_memory_ = std::exchange(src.owns_memory_, false);
}

MappedFile& MappedFile::operator=(MappedFile&& src) {
  if (this !=  &src) {
    unmap();
    ptr_ = std::exchange(src.ptr_, nullptr);
    size_ = std::exchange(src.size_, 0);
    filename_ = std::move(src.filename_);
    owns_memory_ = std::exchange(src.owns_memory_, false);
  }
  return *this;
  
};

MappedFile::MappedFile(u8* data, size_t size, std::string filename, bool owns_memory) : ptr_(data), size_(size), filename_(std::move(filename)), owns_memory_(owns_memory) {}    

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

MappedFile MappedFile::slice(size_t offset, size_t size) const {
  if (offset + size > size_) {
      throw std::out_of_range("Slice out of bounds");
  }
  std::string childName = filename_ + "[" + std::to_string(offset) + ":" + std::to_string(size) + "]";
  return MappedFile(ptr_ + size, size, std::move(childName), false);
  
}

MappedFile::~MappedFile() {
  if (owns_memory_) {
    unmap();
  }
}

std::string_view MappedFile::filename() const { return filename_; }
std::span<const u8> MappedFile::data() const { return std::span(ptr_, size_); }
const u8* MappedFile::raw() const { return ptr_; }
size_t MappedFile::size() const { return size_; }
std::span<u8> MappedFile::data() { return std::span(ptr_, size_); }
u8* MappedFile::raw() { return ptr_; }

} // namespace weld
