#include "elf.h"
#include "src/arch.h"
#include "weld.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <print>
#include <ranges>
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

  if (auto result = elf::get_shdr_table<E>(this->mapped_.data())) {
    elf_shdr_tab_ = result.value();
  } else {
    Fatal() << this->mapped_ << " invalid elf";
  }

  auto symtab_hdr = elf::find_shdr<E>(elf_shdr_tab_, elf::SHT_SYMTAB);
  if (symtab_hdr) {
    auto strtab_hdr = &elf_shdr_tab_[symtab_hdr->sh_link];
    elf_strtab_ = std::string_view(
        reinterpret_cast<char*>(this->mapped_.raw() + strtab_hdr->sh_offset),
        strtab_hdr->sh_size);
  } else {
    static const char empty = '\0';
    elf_strtab_ = std::string_view(&empty, 0);
  }
  auto shstrtab_hdr = &elf_shdr_tab_[ehdr->e_shstrndx];
  elf_shstrtab_ = std::string_view(
      reinterpret_cast<char*>(this->mapped_.raw() + shstrtab_hdr->sh_offset),
      shstrtab_hdr->sh_size);
  input_sections_ = std::vector<InputSection<E>>();

  for (auto& shdr : elf_shdr_tab_) {
    InputSection<E> sec;
    sec.data =
        std::span(reinterpret_cast<u8*>(this->mapped_.raw() + shdr.sh_offset),
                  shdr.sh_size);
    sec.elf_hdr = &shdr;

    if (shdr.sh_type == elf::SHT_RELA) {
      auto relas = std::span(
          reinterpret_cast<elf::Rela<E>*>(this->mapped_.raw() + shdr.sh_offset),
          shdr.sh_size / sizeof(elf::Rela<E>));
      for (elf::Rela<E> rela : relas) {
        auto symbol_name =
            elf_strtab_
                .substr(reinterpret_cast<elf::Sym<E>*>(
                            this->mapped_.raw() +
                            symtab_hdr->sh_offset)[rela.r_info >> 32]
                            .st_name)
                .data(); // FIXME
        sec.relocations.push_back(
            Relocation<E>{.elf_rela = rela, .symbol_name = symbol_name});
      }
      continue;
    }
    input_sections_.push_back(sec);
  }

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
  for (elf::Sym<E>& elf_sym : elf_global_symbols_) {
    auto name = elf_strtab_.substr(elf_sym.st_name).data();
    
    if (elf_sym.st_shndx == elf::SHN_UNDEF) {
      if (!ctx.symbol_map.contains(name)) {
        ctx.symbol_map[name] = Symbol<E>{.esym = &elf_sym, .section = nullptr, .input_section = nullptr, .name = name};
      }
      continue;
    }
    
    if (elf_sym.st_shndx >= elf_shdr_tab_.size()) continue;
    
    auto section_name = elf_shstrtab_.substr(elf_shdr_tab_[elf_sym.st_shndx].sh_name).data();
    auto& merged = ctx.merged_sections[section_name];
    auto& input_sec = input_sections_[elf_sym.st_shndx];
    
    auto sym = Symbol<E>{.esym = &elf_sym, .section = &merged, .input_section = &input_sec, .name = name};
    
    auto it = ctx.symbol_map.find(name);
    if (it == ctx.symbol_map.end()) {
      ctx.symbol_map[name] = sym;
    } else if (it->second.esym->st_shndx == elf::SHN_UNDEF) {
      it->second = sym;
    }
  }
}

template <typename E>
void ObjectFile<E>::merge_sections(Context<E>& ctx) {
  for (InputSection<E>& input_section : input_sections_) {
    auto name = elf_shstrtab_.substr(input_section.elf_hdr->sh_name).data();
    auto& merged = ctx.merged_sections[name];
    
    input_section.offset = merged.data.size();
    
    merged.data.insert(merged.data.end(), input_section.data.begin(), input_section.data.end());
    
    for (auto& rel : input_section.relocations) {
      elf::Rela<E> new_rel = rel.elf_rela;
      new_rel.r_offset += input_section.offset;
      merged.relocations.push_back(Relocation<E>{.elf_rela = new_rel, .symbol_name = rel.symbol_name});
    }
    
    if (input_section.elf_hdr->sh_addralign > merged.alignment) {
      merged.alignment = input_section.elf_hdr->sh_addralign;
    }
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
