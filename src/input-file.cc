#include "elf.h"
#include "src/arch.h"
#include "weld.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <print>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace weld {

template <typename E>
InputFile<E>::InputFile(MappedFile&& mapped)
    : mapped_(std::forward<MappedFile>(mapped)) {}

template <typename E>
std::unique_ptr<InputFile<E>> InputFile<E>::parse(MappedFile&& mapped) {
  if (!elf::is_elf(mapped.data())) {
    Fatal() << mapped << "file is not elf";
  }

  auto file_arch = elf::get_arch(mapped.data());
  if (file_arch == arch::Enum::unsupported) {
    Fatal() << mapped << " architecture is not supported";
  } else if (file_arch != arch::get_enum<E>()) {
    Fatal() << mapped << " the architecture " << file_arch << " expected but ",
        arch::get_enum<E>();
  }

  auto type = reinterpret_cast<elf::Ehdr<E>*>(mapped.raw())->e_type;
  switch (type) {
  case elf::ET_REL:
    return std::make_unique<ObjectFile<E>>(std::move(mapped));
  case elf::ET_DYN:
    return std::make_unique<SharedObjectFile<E>>(std::move(mapped));
  case elf::ET_EXEC:
    Fatal() << mapped << " is executable";
  default:
    Fatal() << mapped << " unknown subtype of elf";
  }
}

template <typename E>
ObjectFile<E>::ObjectFile(MappedFile&& mapped)
    : InputFile<E>(std::move(mapped)) {
  auto ehdr = reinterpret_cast<elf::Ehdr<E>*>(this->mapped_.raw());
  std::span<elf::Shdr<E>> shdr_tab;

  if (auto result = elf::get_shdr_table<E>(this->mapped_.data())) {
    shdr_tab = result.value();
  } else {
    Fatal() << this->mapped_ << " invalid elf";
  }

  auto symtab_hdr = elf::find_shdr<E>(shdr_tab, elf::SHT_SYMTAB);
  auto strtab_hdr = &shdr_tab[symtab_hdr->sh_link];
  auto shstrtab_hdr = &shdr_tab[ehdr->e_shstrndx];
  elf_strtab_ = std::string_view(
      reinterpret_cast<char*>(this->mapped_.raw() + strtab_hdr->sh_offset),
      strtab_hdr->sh_size);
  elf_shstrtab_ = std::string_view(
      reinterpret_cast<char*>(this->mapped_.raw() + shstrtab_hdr->sh_offset),
      shstrtab_hdr->sh_size);
  input_sections_ = std::vector<InputSection<E>>(shdr_tab.size());

  std::transform(
      shdr_tab.begin(), shdr_tab.end(), input_sections_.begin(),
      [this](elf::Shdr<E>& elf_hdr) -> InputSection<E> {
        return {.data = std::span(reinterpret_cast<u8*>(this->mapped_.raw() +
                                                        elf_hdr.sh_offset),
                                  elf_hdr.sh_size),
                .elf_hdr = &elf_hdr};
      });

  if (symtab_hdr) {
    if (auto result = elf::get_symbols_symtab_or_dynsym<E>(this->mapped_.raw(),
                                                           *symtab_hdr)) {
      auto [local, global] = result.value();
      elf_local_symbols_ = local;
      elf_global_symbols_ = global;
    } else {
      Fatal() << std::format("[{}] invalid .symtab section", this->filename());
    }
  }
}

template <typename E>
SharedObjectFile<E>::SharedObjectFile(MappedFile&& mapped)
    : InputFile<E>(std::move(mapped)) {
  std::span<elf::Shdr<E>> shdr_tab;

  if (auto result = elf::get_shdr_table<E>(this->mapped_.data())) {
    shdr_tab = result.value();
  } else {
    Fatal() << this->mapped_ << " invalid elf";
  }

  auto dynsym_hdr = elf::find_shdr<E>(shdr_tab, elf::SHT_DYNSYM);

  if (dynsym_hdr) {
    if (auto result = elf::get_symbols_symtab_or_dynsym<E>(this->mapped_.raw(),
                                                           *dynsym_hdr)) {
      auto [local, global] = result.value();
      elf_local_symbols_ = local;
      elf_global_symbols_ = global;
    } else {
      Fatal() << std::format("[{}] invalid .dynsym section", this->filename());
    }
  }
}

template <typename E>
void ObjectFile<E>::resolve_symbols(Context<E>& ctx) {
  for (const elf::Sym<E>& elf_sym : elf_global_symbols_) {
    auto name = elf_strtab_.substr(elf_sym.st_name).data();
    auto sym = Symbol<E>{.esym = &elf_sym, .name = name};

    if (elf_sym.st_shndx == elf::SHN_UNDEF) {
      if (!ctx.symbol_map.contains(name)) {
        ctx.symbol_map[name] = sym;
      }
      continue;
    }

    Symbol<E>* in_map;
    if (ctx.symbol_map.contains(name)) {
      in_map = &ctx.symbol_map[name];
    } else {
      in_map =
          &(ctx.symbol_map[name] = Symbol<E>{.esym = &elf_sym, .name = name});
    }

    if (sym.esym->st_shndx == elf::SHN_COMMON &&
        in_map->esym->st_shndx == elf::SHN_COMMON) {
      if (sym.esym->st_size > in_map->esym->st_size)
        *in_map = sym;
      continue;
    }

    auto in_map_weak = in_map->esym->st_info & elf::STB_WEAK;
    auto sym_weak = sym.esym->st_info & elf::STB_WEAK;

    if (in_map_weak && !sym_weak) {
      *in_map = sym;
    } else if (!in_map_weak && !sym_weak) {
      Fatal() << std::format("duplicate definition: {}", name);
    }
  }
}

template <typename E>
void ObjectFile<E>::merge_sections(Context<E>& ctx) {
  for (InputSection<E>& input_section : input_sections_) {
    auto name = elf_shstrtab_.substr(input_section.elf_hdr->sh_name).data();
    if (!ctx.merged_sections.contains(name)) {
      ctx.merged_sections[name] = MergedSection<E>{
          .name = name,
          .data = std::vector<u8>(),
      };
    }
    ctx.merged_sections[name].data.insert(ctx.merged_sections[name].data.end(),
                                          input_section.data.begin(),
                                          input_section.data.end());
  }
}

template <typename E>
void SharedObjectFile<E>::resolve_symbols(Context<E>& ctx) {}

template <typename E>
void SharedObjectFile<E>::merge_sections(Context<E>& ctx) {}

template <typename E>
std::ostream& operator<<(std::ostream& out, const InputFile<E>& file) {
  out << file.filename();
  return out;
}

template class InputFile<arch::i386>;
template class InputFile<arch::x86_64>;
template class ObjectFile<arch::i386>;
template class ObjectFile<arch::x86_64>;
template class SharedObjectFile<arch::i386>;
template class SharedObjectFile<arch::x86_64>;
} // namespace weld
