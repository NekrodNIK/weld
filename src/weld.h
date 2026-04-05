#pragma once
#include "ints.h"
#include "src/elf.h"
#include <cassert>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace weld {
template <typename E>
struct Symbol {
  const elf::Sym<E>* esym;
  std::string_view name;
};

template <typename E>
struct Context {
  // NOTE: This is for heterogeneous lookup
  // https://en.cppreference.com/w/cpp/utility/functional.html#Transparent_function_objects
  struct string_hash {
    using is_transparent = void;
    size_t operator()(const char* txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(std::string_view txt) const {
      return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(const std::string& txt) const {
      return std::hash<std::string>{}(txt);
    }
  };
  // NOTE: I guess it needs to be protected with a lock or use a lock-free data
  // structure
  std::unordered_map<std::string_view, Symbol<E>, string_hash,
                            std::equal_to<>>
      symbol_map;
};

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

template<typename E>
class InputFile {
protected:
  MappedFile mapped_;
  InputFile(MappedFile&& mapped);

public:
  static std::unique_ptr<InputFile> parse(MappedFile&& mapped);
  std::string_view filename() const { return mapped_.filename(); }

  void sy
};

template <typename E>
class ObjectFile : public InputFile<E> {
  std::span<elf::Sym<E>> elf_local_symbols_;
  std::span<elf::Sym<E>> elf_global_symbols_;
  char* strtab;

public:
  ObjectFile(MappedFile&& mapped);
};

template <typename E>
class SharedObjectFile : public InputFile<E> {
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
