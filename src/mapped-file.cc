#include "weld.h"
#include <filesystem>
#include <ostream>
#include <span>
#include <string_view>
#include <utility>

namespace weld {
namespace fs = ::std::filesystem;

MappedFile::MappedFile(MappedFile&& src) {
  ptr_ = std::exchange(src.ptr_, nullptr);
  size_ = std::exchange(src.size_, 0);
  filename_ = std::move(src.filename_);
  owns_ = std::exchange(src.owns_, false);
}

MappedFile& MappedFile::operator=(MappedFile&& src) {
  if (this != &src) {
    ptr_ = std::exchange(src.ptr_, nullptr);
    size_ = std::exchange(src.size_, 0);
    filename_ = std::move(src.filename_);
    owns_ = std::exchange(src.owns_, false);
  }
  return *this;
};

MappedFile MappedFile::open(const fs::path& path) {
  MappedFile file;
  file.filename_ = path.filename();

  if (!fs::exists(path)) {
    Fatal() << file << "not found";
  } else if (fs::is_directory(path)) {
    Fatal() << file << "is directory";
  }

  if (!file.map_(path.c_str())) {
    Fatal() << file << "unable to open";
  }

  return file;
}

MappedFile MappedFile::slice(size_t offset, size_t size) const {
  if (offset + size > size_) {
    throw std::out_of_range("Slice out of bounds");
  }
  // std::string child_name = filename_ + "[" + std::to_string(offset) + ":" +
  //                          std::to_string(size) + "]";
  MappedFile mapped;
  mapped.ptr_ = ptr_ + offset;
  mapped.size_ = size;
  mapped.owns_ = false;
  mapped.filename_ = filename_;
  return mapped;
}

MappedFile::~MappedFile() {
  if (owns_) {
    unmap_();
  }
}

std::string_view MappedFile::filename() const { return filename_; }
std::span<const u8> MappedFile::data() const { return std::span(ptr_, size_); }
const u8* MappedFile::raw() const { return ptr_; }
size_t MappedFile::size() const { return size_; }
std::span<u8> MappedFile::data() { return std::span(ptr_, size_); }
u8* MappedFile::raw() { return ptr_; }

std::ostream& operator<<(std::ostream& out, const MappedFile& mapped) {
  out << mapped.filename();
  return out;
}
} // namespace weld
