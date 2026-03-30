// NOTE: Supports only little-endian architectures.
// However, this is not currently a problem,
// as only the i386 and x86_64 architectures are supported.
#pragma once
#include "ints.h"
#include <type_traits>

namespace weld::elf {
enum : u32 {
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

enum : u32 { EV_CURRENT = 1 };

struct i386 {
  static constexpr bool is_64 = false;
  static constexpr u32 e_machine = EM_386;
  static constexpr unsigned char ei_class = ELFCLASS32;
};
struct x86_64 {
  static constexpr bool is_64 = true;
  static constexpr u32 e_machine = EM_X86_64;
  static constexpr unsigned char ei_class = ELFCLASS64;
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
using Word = std::conditional_t<E::is_64, u64, i32>;
template <typename E>
using Sword = std::conditional_t<E::is_64, i64, i32>;
template <typename E>
struct Ehdr {
  unsigned char e_ident[EI_IDENT];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  Word<E> e_entry;
  Word<E> e_phoff;
  Word<E> e_shoff;
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
  Word<E> sh_flags;
  Word<E> sh_addr;
  Word<E> sh_offset;
  Word<E> sh_size;
  u32 sh_link;
  u32 sh_info;
  Word<E> sh_addralign;
  Word<E> sh_entsize;
};
template <typename E>
  requires(!E::is_64)
struct Phdr<E> {
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
  requires(!E::is_64)
struct Sym<E> {
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
  Sword<E> d_tag;
  Word<E> d_val;
};
template <typename E>
struct Rel {
  Word<E> r_offset;
  Word<E> r_info;
};
template <typename E>
struct Rela {
  Word<E> r_offset;
  Word<E> r_info;
  Sword<E> r_addend;
};

} // namespace weld::elf
