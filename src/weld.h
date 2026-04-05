#pragma once
#include "ints.h"
#include "src/elf.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace weld {
class MappedFile {
  u8* ptr_;
  size_t size_;
  bool owns_;
  std::string filename_;

  bool map_(const char* path);
  void unmap_();
  MappedFile() : ptr_(nullptr), size_(0), owns_(false) {};

public:
  static MappedFile open(const std::filesystem::path& path);
  MappedFile slice(size_t offset, size_t size) const;

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

  friend std::ostream& operator<<(std::ostream& out, const MappedFile& mapped);
};

template <typename E>
struct Symbol {
  const elf::Sym<E>* esym;
  std::string_view name;
};

template <typename E>
struct InputSection {
  std::span<u8> data;
  elf::Shdr<E>* elf_hdr;
};

template <typename E>
struct MergedSection {
  std::string name;
  std::vector<u8> data;
};

template <typename E>
struct Context {
  // NOTE: This for heterogeneous lookup
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
  // NOTE: I guess it needs to be protected with a lock
  // or use a lock-free data structure
  std::unordered_map<std::string, Symbol<E>, string_hash> symbol_map;
  std::unordered_map<std::string, MergedSection<E>, string_hash>
      merged_sections; // FIXME: split into two stages, merge and output
};

template <typename E>
class InputFile {
protected:
  MappedFile mapped_;
  InputFile(MappedFile&& mapped);

public:
  virtual ~InputFile() = default;
  static std::unique_ptr<InputFile> parse(MappedFile&& mapped);
  std::string_view filename() const { return mapped_.filename(); }
  friend std::ostream& operator<<(std::ostream& out, const InputFile& file);

  virtual void resolve_symbols(Context<E>& ctx) = 0;
  virtual void merge_sections(Context<E>& ctx) = 0;
  virtual void resolve_relocations(Context<E>& ctx) = 0;
};

template <typename E>
class ObjectFile : public InputFile<E> {
  std::span<elf::Sym<E>> elf_local_symbols_;
  std::span<elf::Sym<E>> elf_global_symbols_;
  std::string_view elf_strtab_;
  std::string_view elf_shstrtab_;
  std::vector<InputSection<E>> input_sections_;

public:
  ObjectFile(MappedFile&& mapped);
  void resolve_symbols(Context<E>& ctx) override;
  void merge_sections(Context<E>& ctx) override;
};

template <typename E>
class SharedObjectFile : public InputFile<E> {
  std::span<elf::Sym<E>> elf_local_symbols_;
  std::span<elf::Sym<E>> elf_global_symbols_;

public:
  SharedObjectFile(MappedFile&& mapped);
  void resolve_symbols(Context<E>& ctx) override;
  void merge_sections(Context<E>& ctx) override;
};

template <typename E>
class OutputFile {
public:
  void resolve_relocations(Context<E>& ctx);
};

// TODO: std::print support
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
