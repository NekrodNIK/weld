// NOTE: Supports only little-endian architectures.
// However, this is not currently a problem,
// as only the i386 and x86_64 architectures are supported.
#pragma once
#include "arch.h"
#include "ints.h"
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>

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
  SHF_WRITE = 0x1,
  SHF_ALLOC = 0x2,
  SHF_EXECINSTR = 0x4,
};

enum : u32 {
  STT_OBJECT = 1,
  STT_FUNC = 2,
  STT_SECTION = 3,
  STT_FILE = 4,
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

enum : u8 {
  STB_WEAK = 2,
  STB_LOCAL = 1,
  STB_GLOBAL = 2,
  STB_GNU_UNIQUE = 12,
};

enum : u32 {
  EV_CURRENT = 1,
};

enum : u32 {
  PT_NONE = 0,
  PT_LOAD = 1,
};

enum : u32 {
  PF_X = 0x1,
  PF_W = 0x2,
  PF_R = 0x4,
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
  u8 st_bind() const { return st_info >> 4; }
  u8 st_type() const { return st_info & 0xf; }
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
  u8 st_bind() const { return st_info >> 4; }
  u8 st_type() const { return st_info & 0xf; }
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
  size_t r_sym() {
    if constexpr (E::is_64) {
      return r_info >> 32;
    } else {
      return r_info >> 8;
    }
  }
};

bool is_elf(std::span<const u8> file);
arch::Enum get_arch(std::span<const u8> file);
template <typename E>
Ehdr<E>& get_ehdr(std::span<u8> file);
template <typename E>
std::optional<std::span<Shdr<E>>> get_shdr_tab(std::span<u8> file);
template <typename E>
std::optional<std::pair<std::span<Sym<E>>, size_t>>
get_symtab(std::span<u8> file);
template <typename E>
char* get_strtab(std::span<u8> file);
template <typename E>
char* get_shstrtab(std::span<u8> file);
template <typename E>
std::span<Rela<E>> get_rela_tab(std::span<u8> file, Shdr<E>& shdr);
} // namespace weld::elf
