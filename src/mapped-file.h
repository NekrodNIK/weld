#include "src/errors.h"
#include "src/ints.h"
#include <cstddef>
#include <filesystem>
#include <format>
#include <span>
#include <string>

namespace weld {
class MappedFile {
  u8* ptr_;
  std::size_t size_;
  bool owns_;
  std::string filename_;

  bool map_(const char* path);
  void unmap_();
  MappedFile() : ptr_(nullptr), size_(0), owns_(false) {};

public:
  static MappedFile open(const std::filesystem::path& path);
  static MappedFile from_span(std::span<u8> span, bool owns = false, std::string filename = "");
  MappedFile slice(size_t offset, size_t size) const;
  MappedFile slice(size_t offset) const;

  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& src);
  MappedFile& operator=(MappedFile&& src);
  ~MappedFile();

  std::string_view filename() const;
  std::span<const u8> data() const;
  const u8* raw() const;
  size_t size() const;
  std::span<u8> data();
  u8* raw();
};

std::ostream& operator<<(std::ostream& out, const MappedFile& mapped);
} // namespace weld
template <>
struct std::formatter<weld::MappedFile>
    : public weld::ostream_formatter<weld::MappedFile> {};
