#include "elf.h"
#include "src/arch.h"
#include "src/errors.h"
#include "weld.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <ostream>
#include <span>
#include <utility>
#include <vector>

namespace weld {
template <typename E>
InputFile<E>::InputFile(MappedFile&& mapped) : mapped_(std::move(mapped)) {}

template <typename E>
std::unique_ptr<InputFile<E>> InputFile<E>::parse(MappedFile&& mapped) {
  if (!elf::is_elf(mapped.data())) {
    Fatal().println("[{}] file is not elf", mapped);
  }

  auto file_arch = elf::get_arch(mapped.data());
  if (file_arch != arch::get_enum<E>()) {
    Fatal().println("[{}] the architecture {} expected but {}", mapped,
                    file_arch, arch::get_enum<E>());
  }

  auto type = reinterpret_cast<elf::Ehdr<E>*>(mapped.raw())->e_type;
  switch (type) {
  case elf::ET_REL:
    return std::make_unique<ObjectFile<E>>(std::move(mapped));
  case elf::ET_DYN:
    return std::make_unique<SharedObjectFile<E>>(std::move(mapped));
  case elf::ET_EXEC:
    Fatal().println("[{}] is executable", mapped);
  default:
    Fatal().println("[{}] unknown subtype of elf", mapped);
  }
}

template <typename E>
ObjectFile<E>::ObjectFile(MappedFile&& mapped)
    : InputFile<E>(std::move(mapped)) {
  if (auto result = elf::get_shdr_tab<E>(this->mapped_.data())) {
    shdr_tab_ = result.value();
  } else {
    Fatal().println("cannot read section headers: {}", this->mapped_);
  }

  if (auto result = elf::get_symtab<E>(this->mapped_.data())) {
    auto [symtab, first_non_local] = result.value();
    local_symtab_ = symtab.subspan(0, first_non_local);
    non_local_symtab_ = symtab.subspan(first_non_local);
  } else {
    Warn().println("cannot read .symtab: {}", this->mapped_);
  }

  if (auto ptr = elf::get_strtab<E>(this->mapped_.data())) {
    strtab_ = ptr;
  } else {
    Warn().println("cannot read .strtab: {}", this->mapped_);
  }

  if (auto ptr = elf::get_shstrtab<E>(this->mapped_.data())) {
    shstrtab_ = ptr;
  } else {
    Warn().println("cannot read .shstrtab: {}", this->mapped_);
  }

  for (auto& shdr : shdr_tab_) {
    char* name;

    if (shdr.sh_type == elf::SHT_PROGBITS) {
      name = shstrtab_ + shdr.sh_name;
    } else if (shdr.sh_type == elf::SHT_RELA) {
      name = shstrtab_ + shdr_tab_[shdr.sh_info].sh_name;
    } else {
      continue;
    }

    if (!sections_.contains(name)) {
      sections_[name] = {.name = name};
    }

    InputSection<E>& section = sections_[name];
    if (shdr.sh_type == elf::SHT_PROGBITS || shdr.sh_type == elf::SHT_NOBITS) {
      section.data =
          std::span<u8>(this->mapped_.raw() + shdr.sh_offset, shdr.sh_size);
      section.align = shdr.sh_addralign;
    } else if (shdr.sh_type == elf::SHT_RELA) {
      section.rela_tab = elf::get_rela_tab(this->mapped_.data(), shdr);
    } else {
      continue;
    }
  }
}

template <typename E>
void ObjectFile<E>::resolve_symbols(Context<E>& ctx) {
  for (elf::Sym<E>& elf_struct : non_local_symtab_) {
    auto name = strtab_ + elf_struct.st_name;

    if (elf_struct.st_shndx >= shdr_tab_.size()) {
      Warn().println("Invalid section index: {}: {}", *this, name);
      continue;
    }
    auto input_section =
        elf_struct.st_shndx != elf::SHN_UNDEF
            ? &sections_[shstrtab_ + shdr_tab_[elf_struct.st_shndx].sh_name]
            : nullptr;
    auto new_symbol =
        Symbol<E>{.name = name,
                  .input_section = input_section,
                  .output_section = nullptr,
                  .is_weak = (elf_struct.st_bind() == elf::STB_WEAK ||
                              elf_struct.st_bind() == elf::STB_GNU_UNIQUE),
                  .addr = elf_struct.st_value};
    if (!ctx.symbol_map.contains(name) || !ctx.symbol_map[name].is_defined()) {
      ctx.symbol_map[name] = new_symbol;
      continue;
    }
    if (!new_symbol.is_defined()) {
      continue;
    }

    Symbol<E>& symbol = ctx.symbol_map[name];
    if (symbol.is_weak && !new_symbol.is_weak) {
      symbol = new_symbol;
    } else if (!symbol.is_weak && new_symbol.is_weak) {
      continue;
    } else if (!symbol.is_weak && !new_symbol.is_weak) {
      Fatal().println("duplicate symbol: {}: {}", *this, name);
    }
  }

  for (elf::Sym<E>& elf_struct : local_symtab_) {
    if (elf_struct.st_type() == elf::STT_FILE)
      continue;
    if (elf_struct.st_type() == elf::STT_SECTION)
      continue;
    auto name = strtab_ + elf_struct.st_name;
    InputSection<E>* input_section;
    if (elf_struct.st_shndx >= shdr_tab_.size()) {
      input_section = &sections_[".text"]; // FIXME
    } else {
      input_section =
          elf_struct.st_shndx != elf::SHN_UNDEF
              ? &sections_[shstrtab_ + shdr_tab_[elf_struct.st_shndx].sh_name]
              : nullptr;
    }
    ctx.local_symbols.push_back(
        Symbol<E>{.name = name,
                  .input_section = input_section,
                  .output_section = nullptr,
                  .is_weak = (elf_struct.st_bind() == elf::STB_WEAK ||
                              elf_struct.st_bind() == elf::STB_GNU_UNIQUE),
                  .addr = elf_struct.st_value});
  }
}

template <typename E>
void ObjectFile<E>::merge_sections(Context<E>& ctx) {
  for (auto& [name, input] : sections_) {
    if (!ctx.merged_sections.contains(name)) {
      ctx.merged_sections[name] = MergedSection<E>{};
    }

    MergedSection<E>& merged = ctx.merged_sections[name];

    input.offset = merged.data.size();
    merged.data.insert(merged.data.end(), input.data.begin(), input.data.end());

    for (elf::Rela<E> rela : input.rela_tab) {
      auto ind = rela.r_sym();
      auto& symtab =
          ind < local_symtab_.size() ? local_symtab_ : non_local_symtab_;
      auto symbol_name = strtab_ + symtab[ind].st_name;

      rela.r_offset += input.offset;
      merged.relocations.push_back({
          .rela = rela,
          .symbol_name = symbol_name,
      });
    }

    if (input.align > merged.align) {
      merged.align = input.align;
    }
  }
}

template <typename E>
SharedObjectFile<E>::SharedObjectFile(MappedFile&& mapped)
    : InputFile<E>(std::move(mapped)) {
  Fatal().println("unimplemented yet");
}

template <typename E>
void SharedObjectFile<E>::resolve_symbols(Context<E>& ctx) {
  Fatal().println("unimplemented yet");
}

template <typename E>
void SharedObjectFile<E>::merge_sections(Context<E>& ctx) {
  Fatal().println("unimplemented yet");
}

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
