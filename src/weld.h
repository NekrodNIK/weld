#pragma once
#include "ints.h"
#include "src/elf.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>

namespace weld {

class MappedFile {
  u8* ptr_;
  size_t size_;
  std::string filename_;

  MappedFile() : ptr_(nullptr), size_(0) {};
  bool map(const char* path);
  void unmap();

public:
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& src);
  MappedFile& operator=(MappedFile&& src);
  ~MappedFile();
  static std::optional<MappedFile> open(const std::filesystem::path& path);

  std::string_view filename() const;
  std::span<const u8> data() const;
  const u8* raw() const;
  size_t size() const;

  std::span<u8> data();
  u8* raw();
};

class InputFile {
protected:
  MappedFile mapped_;
  InputFile(MappedFile&& mapped);

public:
  static std::unique_ptr<InputFile> parse(MappedFile&& mapped);
  std::string_view filename() const { return mapped_.filename(); }
};

template <typename E>
class ObjectFile : public InputFile {
  std::span<elf::Sym<E>> elf_local_symbols_;
  std::span<elf::Sym<E>> elf_global_symbols_;

public:
  ObjectFile(MappedFile&& mapped);
};

template <typename E>
class SharedObjectFile : public InputFile {
  std::span<elf::Sym<E>> elf_local_symbols_;
  std::span<elf::Sym<E>> elf_global_symbols_;

public:
  SharedObjectFile(MappedFile&& mapped);
};

class Fatal {
  std::ostream& out;

public:
  Fatal();
  [[noreturn]] ~Fatal();
  template <typename T>
  Fatal operator<<(T&& val) {
    out << std::forward<T>(val);
    return *this;
  };
};

class Error {
  std::ostream& out;

public:
  Error();
  template <typename T>
  Error operator<<(T&& val) {
    out << std::forward<T>(val);
    return *this;
  };
};

class Warn {
  std::ostream& out;

public:
  Warn();
  template <typename T>
  Warn operator<<(T&& val) {
    out << std::forward<T>(val);
    return *this;
  };
};

} // namespace weld
