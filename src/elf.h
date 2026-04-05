// NOTE: Supports only little-endian architectures.
// However, this is not currently a problem,
// as only the i386 and x86_64 architectures are supported.
#pragma once
#include "ints.h"
#include "arch.h"
#include <cassert>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace weld::elf {
enum : u32 {
  EI_MAG0 = 0,
  EI_MAG1 = 1,
  EI_MAG2 = 2,
  EI_MAG3 = 3,
  EI_CLASS = 4,
  EI_DATA = 5,
  EI_VERSION = 6,
  EI_OSABI = 7,
  EI_ABIVERSION = 8,
  EI_IDENT = 16,
};

enum : u32 {
  ELFCLASS32 = 1,
  ELFCLASS64 = 2,
};

enum : u32 {
  ET_NONE = 0,
  ET_REL = 1,
  ET_EXEC = 2,
  ET_DYN = 3,
};

enum : u32 {
  EM_NONE = 0,
  EM_386 = 3,
  EM_X86_64 = 62,
};

enum : u32 {
  SHT_NULL = 0,
  SHT_PROGBITS = 1,
  SHT_SYMTAB = 2,
  SHT_STRTAB = 3,
  SHT_RELA = 4,
  SHT_HASH = 5,
  SHT_DYNAMIC = 6,
  SHT_NOTE = 7,
  SHT_NOBITS = 8,
  SHT_REL = 9,
  SHT_SHLIB = 10,
  SHT_DYNSYM = 11,
};

enum : u32 {
  SHN_UNDEF = 0,
  SHN_ABS = 0xFFF1,
  SHN_COMMON = 0xFFF2,
};

enum : u32 {
  STB_WEAK = 2,
  STB_LOCAL = 1,
  STB_GLOBAL = 2,
};

enum : u32 {
  EV_CURRENT = 1,
};

template <typename E>
struct Ehdr;
template <typename E>
struct Shdr;
template <typename E>
struct Phdr;
template <typename E>
struct Sym;
template <typename E>
struct Dyn;
template <typename E>
struct Rel;
template <typename E>
struct Rela;

template <typename E>
using word = std::conditional_t<E::is_64, u64, i32>;
template <typename E>
using sword = std::conditional_t<E::is_64, i64, i32>;
template <typename E>
struct Ehdr {
  unsigned char e_ident[EI_IDENT];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  word<E> e_entry;
  word<E> e_phoff;
  word<E> e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
};
template <typename E>
struct Shdr {
  u32 sh_name;
  u32 sh_type;
  word<E> sh_flags;
  word<E> sh_addr;
  word<E> sh_offset;
  word<E> sh_size;
  u32 sh_link;
  u32 sh_info;
  word<E> sh_addralign;
  word<E> sh_entsize;
};
template <typename E>
struct Phdr {
  u32 p_type;
  u32 p_offset;
  u32 p_vaddr;
  u32 p_paddr;
  u32 p_filesz;
  u32 p_memsz;
  u32 p_flags;
  u32 p_align;
};
template <typename E>
  requires(E::is_64)
struct Phdr<E> {
  u32 p_type;
  u32 p_flags;
  u64 p_offset;
  u64 p_vaddr;
  u64 p_paddr;
  u64 p_filesz;
  u64 p_memsz;
  u64 p_align;
};
template <typename E>
struct Sym {
  u32 st_name;
  u32 st_value;
  u32 st_size;
  unsigned char st_info;
  unsigned char st_other;
  u16 st_shndx;
};
template <typename E>
  requires(E::is_64)
struct Sym<E> {
  u32 st_name;
  unsigned char st_info;
  unsigned char st_other;
  u16 st_shndx;
  u64 st_value;
  u64 st_size;
};
template <typename E>
struct Dyn {
  sword<E> d_tag;
  word<E> d_val;
};
template <typename E>
struct Rel {
  word<E> r_offset;
  word<E> r_info;
};
template <typename E>
struct Rela {
  word<E> r_offset;
  word<E> r_info;
  sword<E> r_addend;
};

bool is_elf(std::span<const u8> mem);
arch::Enum get_arch(std::span<const u8> mem);

template <typename E>
std::optional<std::span<Shdr<E>>> get_shdr_table(std::span<u8> mem) {
  elf::Ehdr<E>* ehdr = reinterpret_cast<elf::Ehdr<E>*>(mem.data());
  if (mem.size() < ehdr->e_shoff + ehdr->e_shnum * sizeof(Shdr<E>)) {
    return {};
  }
  // FIXME: It's probably best to check the alignment of the addresses in the file
  // before converting types (you never know what address was written to the file).
  return std::span(reinterpret_cast<Shdr<E>*>(mem.data() + ehdr->e_shoff),
                   ehdr->e_shnum);
}

template <typename E>
Shdr<E>* find_shdr(std::span<elf::Shdr<E>> shdr_tab, u32 type) {
  for (Shdr<E>& shdr : shdr_tab)
    if (shdr.sh_type == type)
      return &shdr;
  return nullptr;
}

template <typename E>
std::optional<std::pair<std::span<Sym<E>>, std::span<Sym<E>>>>
get_symbols_symtab_or_dynsym(u8* mem, elf::Shdr<E>& shdr) {
  if (shdr.sh_entsize != sizeof(elf::Sym<E>)) {
    return {};
  }

  auto symbols = std::span(reinterpret_cast<elf::Sym<E>*>(mem + shdr.sh_offset),
                           shdr.sh_size / shdr.sh_entsize);
  auto result = std::pair(symbols.subspan(0, shdr.sh_info - 1),
                          symbols.subspan(shdr.sh_info));
  return result;
}

} // namespace weld::elf
