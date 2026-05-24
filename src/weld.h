#pragma once
#include "elf.h"
#include "ints.h"
#include "mapped-file.h"
#include "thread-pool.h"
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace weld {
template <typename E>
class Context;
template <typename E>
class InputFile;
template <typename E>
class ObjectFile;
template <typename E>
class SharedObjectFile;
class ArchiveMember;
template <typename E>
class ArchiveFile;
template <typename E>
class InputSection;
template <typename E>
class MergedSection;
template <typename E>
class OutputSection;
template <typename E>
class Symbol;
template <typename E>
class Relocation;

struct string_hash;

template <typename E>
class InputFile {
protected:
  MappedFile mapped_;
  InputFile(MappedFile&& mapped);

public:
  InputFile(InputFile&&) = default;
  InputFile& operator=(InputFile&&) = default;
  virtual ~InputFile() = default;

  static std::unique_ptr<InputFile> parse(MappedFile&& mapped);
  std::string_view filename() const { return mapped_.filename(); }
  virtual void resolve_symbols(Context<E>& ctx) = 0;
  virtual void merge_sections(Context<E>& ctx) = 0;
};

template <typename E>
class ObjectFile : public InputFile<E> {
  std::span<elf::Shdr<E>> shdr_tab_;
  std::span<elf::Sym<E>> local_symtab_;
  std::span<elf::Sym<E>> non_local_symtab_;
  char* strtab_;
  char* shstrtab_;
  std::unordered_map<std::string, InputSection<E>, string_hash> sections_;

public:
  ObjectFile(MappedFile&& mapped);
  void resolve_symbols(Context<E>& ctx) override;
  void merge_sections(Context<E>& ctx) override;
  bool has_non_local(std::string_view name);
};

template <typename E>
class SharedObjectFile : public InputFile<E> {
public:
  SharedObjectFile(MappedFile&& mapped);
  void resolve_symbols(Context<E>& ctx) override;
  void merge_sections(Context<E>& ctx) override;
};

class ArchiveMember {
  std::string_view name_;
  std::span<u8> mem_;
  public:
    static std::optional<ArchiveMember> parse(std::span<u8> mem);
    std::string_view name() const;
    std::span<u8> mem() const;
    size_t total_size() const;
};

template <typename E>
class ArchiveFile : public InputFile<E> {
  std::vector<ArchiveMember> members;
  std::vector<ObjectFile<E>> loaded_objs;

public:
  ArchiveFile(MappedFile&& mapped);
  void resolve_symbols(Context<E>& ctx) override;
  void merge_sections(Context<E>& ctx) override;
  static bool is_archive(std::span<u8> mem);
};

template <typename E>
class InputSection {
public:
  std::string name;
  std::span<u8> data;
  std::span<elf::Rel<E>> rel_tab;
  size_t offset;
  size_t align;

  void scan_relocations(Context<E>& ctx);
  void apply_reloc_alloc(Context<E>& ctx);
  void apply_reloc_nonalloc(Context<E>& ctx);
  void write_to(Context<E>& ctx, std::span<u8> buf);
};

template <typename E>
class MergedSection {
public:
  std::vector<u8> data;
  std::vector<InputSection<E>*> input_sections;
  std::vector<Relocation<E>> relocations;
  size_t align;
};

template <typename E>
class OutputSection {
public:
  std::string name;
  std::vector<u8> data;
  size_t addr;
  std::vector<Relocation<E>> relocations;
};

template <typename E>
class Symbol {
public:
  std::string name;
  InputSection<E>* input_section;
  OutputSection<E>* output_section;
  bool is_weak;
  size_t addr;
  bool is_defined() const { return input_section; }
};

template <typename E>
class Relocation {
public:
  elf::Rel<E> rel;
  std::string symbol_name;
};

template <typename E>
class OutputFile {
public:
  void resolve_relocations(Context<E>& ctx);
  void write(Context<E>& ctx, const std::filesystem::path& file);
};

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

template <typename E>
class Context {
public:
  std::unordered_map<std::string, Symbol<E>, string_hash> symbol_map;
  std::unordered_map<std::string, MergedSection<E>, string_hash> merged_sections;
  std::vector<OutputSection<E>> output_sections;
  std::unordered_map<std::string, size_t, string_hash> output_sec_ind;
  std::vector<Symbol<E>> local_symbols;
  
  bool is_relocatable = false;
  size_t start_addr = 0x400000;
  
  ThreadPool thread_pool;
  Tasks tasks;
};

template <typename T, typename V>
auto align_addr(const T& addr, const V& align) {
  if (align <= 1) return addr;
  return (addr + align - 1) & ~(align - 1);
}
} // namespace weld

template <typename E>
struct std::formatter<weld::InputFile<E>>
    : public weld::ostream_formatter<weld::InputFile<E>> {};
template <typename E>
struct std::formatter<weld::ObjectFile<E>>
    : public weld::ostream_formatter<weld::ObjectFile<E>> {};
template <typename E>
struct std::formatter<weld::SharedObjectFile<E>>
    : public weld::ostream_formatter<weld::SharedObjectFile<E>> {};
