#include "elf.h"
#include "src/arch.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <utility>

namespace weld::elf {
bool is_elf(std::span<const u8> mem) {
  if (mem.size() < 4) {
    return false;
  }
  return (mem[EI_MAG0] == '\x7f') && (mem[EI_MAG1] == 'E') &&
         (mem[EI_MAG2] == 'L') && (mem[EI_MAG3] == 'F');
}

arch::Enum get_arch(std::span<const u8> mem) {
  if (mem.size() < EI_IDENT + 4) {
    return arch::Enum::unsupported;
  }

  u16 e_machine = *(u16*)(mem.data() + EI_IDENT + 2);
  switch (e_machine) {
  case EM_386:
    return arch::Enum::i386;
  case EM_X86_64:
    return arch::Enum::x86_64;
  default:
    return arch::Enum::unsupported;
  }
}

template <typename E>
Ehdr<E>& get_ehdr(std::span<u8> file) {
  return *reinterpret_cast<elf::Ehdr<E>*>(file.data());
}

template <typename E>
std::optional<std::span<Shdr<E>>> get_shdr_tab(std::span<u8> file) {
  auto& ehdr = get_ehdr<E>(file);
  if (file.size() < ehdr.e_shoff + ehdr.e_shnum * sizeof(Shdr<E>))
    return {};
  auto ptr = reinterpret_cast<Shdr<E>*>(file.data() + ehdr.e_shoff);
  return std::span(ptr, ehdr.e_shnum);
}

template <typename E>
std::optional<std::reference_wrapper<Shdr<E>>> get_shdr(std::span<u8> file,
                                                        u32 type) {
  return get_shdr_tab<E>(file).and_then(
      [type](auto shdr_tab) -> std::optional<std::reference_wrapper<Shdr<E>>> {
        auto it = std::ranges::find(shdr_tab, type, &Shdr<E>::sh_type);
        if (it == shdr_tab.end())
          return std::nullopt;
        return std::ref(*it);
      });
}

template <typename E>
std::optional<std::pair<std::span<Sym<E>>, size_t>>
get_symtab(std::span<u8> file) {
  return get_shdr<E>(file, SHT_SYMTAB)
      .and_then([&file](auto shdr_ref)
                    -> std::optional<std::pair<std::span<Sym<E>>, size_t>> {
        auto& shdr = shdr_ref.get();
        if (shdr.sh_entsize != sizeof(elf::Sym<E>))
          return std::nullopt;
        return std::make_pair(
            std::span(reinterpret_cast<Sym<E>*>(file.data() + shdr.sh_offset),
                      shdr.sh_size / shdr.sh_entsize),
            shdr.sh_info);
      });
}

template <typename E>
char* get_strtab(std::span<u8> file) {
  auto shdr_tab_opt = get_shdr_tab<E>(file);
  if (!shdr_tab_opt)
    return nullptr;
  auto shdr_tab = shdr_tab_opt.value();

  return get_shdr<E>(file, SHT_SYMTAB)
      .and_then([&shdr_tab, &file](auto shdr_ref) -> std::optional<char*> {
        auto symtab_shdr = shdr_ref.get();
        auto strtab_hdr = shdr_tab[symtab_shdr.sh_link];
        return reinterpret_cast<char*>(file.data() + strtab_hdr.sh_offset);
      })
      .value_or(nullptr);
}

template <typename E>
char* get_shstrtab(std::span<u8> file) {
  auto ehdr = get_ehdr<E>(file);
  return get_shdr_tab<E>(file)
      .and_then([&file, &ehdr](auto shdr_tab) -> std::optional<char*> {
        auto shstrtab_hdr = shdr_tab[ehdr.e_shstrndx];
        return reinterpret_cast<char*>(file.data() + shstrtab_hdr.sh_offset);
      })
      .value_or(nullptr);
}

template <typename E>
std::span<Rela<E>> get_rela_tab(std::span<u8> file, Shdr<E>& shdr) {
  return std::span(
      reinterpret_cast<elf::Rela<E>*>(file.data() + shdr.sh_offset),
      shdr.sh_size / sizeof(elf::Rela<E>));
}

template Ehdr<arch::i386>& get_ehdr(std::span<u8> file);
template Ehdr<arch::x86_64>& get_ehdr(std::span<u8> file);
template std::optional<std::span<Shdr<arch::i386>>>
get_shdr_tab(std::span<u8> file);
template std::optional<std::span<Shdr<arch::x86_64>>>
get_shdr_tab(std::span<u8> file);
template std::optional<std::pair<std::span<Sym<arch::i386>>, size_t>>
get_symtab(std::span<u8> file);
template std::optional<std::pair<std::span<Sym<arch::x86_64>>, size_t>>
get_symtab(std::span<u8> file);
template char* get_strtab<arch::i386>(std::span<u8> file);
template char* get_strtab<arch::x86_64>(std::span<u8> file);
template char* get_shstrtab<arch::i386>(std::span<u8> file);
template char* get_shstrtab<arch::x86_64>(std::span<u8> file);
template std::span<Rela<arch::i386>> get_rela_tab(std::span<u8> file,
                                                  Shdr<arch::i386>& shdr);
template std::span<Rela<arch::x86_64>> get_rela_tab(std::span<u8> file,
                                                    Shdr<arch::x86_64>& shdr);
} // namespace weld::elf
