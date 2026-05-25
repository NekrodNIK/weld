#include "elf.h"
#include "src/arch.h"
#include "src/errors.h"
#include "weld.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace weld {
template <typename E>
InputFile<E>::InputFile(MappedFile&& mapped) : mapped_(std::move(mapped)) {}

template <typename E>
std::unique_ptr<InputFile<E>> InputFile<E>::parse(MappedFile&& mapped) {
  if (ArchiveFile<E>::is_archive(mapped.data())) {
    return std::make_unique<ArchiveFile<E>>(std::move(mapped));
  }

  if (!elf::is_elf(mapped.data())) {
    Fatal().println("[{}] file is not elf", mapped);
  }

  auto file_arch = elf::get_arch_tag(mapped.data());
  if (file_arch != E::tag) {
    Fatal().println("[{}] the architecture {} expected but {}", mapped,
                    file_arch, E::tag);
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
      if (shdr.sh_info >= shdr_tab_.size()) {
        Warn().println("invalid sh_info {} for RELA section {} in {}",
                       shdr.sh_info, shdr.sh_name, *this);
        continue;
      }
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
      section.rel_tab = elf::get_rel_tab(this->mapped_.data(), shdr);
    } else {
      continue;
    }
  }
}

template <typename E>
void ObjectFile<E>::resolve_symbols(Context<E>& ctx) {
  constexpr uint16_t SHN_LORESERVE = 0xff00;
  
  for (elf::Sym<E>& elf_struct : non_local_symtab_) {
    const char* name_ptr = strtab_ + elf_struct.st_name;
    std::string name(name_ptr);
    
    InputSection<E>* input_section = nullptr;
    
    if (elf_struct.st_shndx == elf::SHN_UNDEF) {
        input_section = nullptr;
    } else if (elf_struct.st_shndx < shdr_tab_.size()) {
        size_t idx = elf_struct.st_shndx;
        const char* sec_name = shstrtab_ + shdr_tab_[idx].sh_name;
        auto it = sections_.find(sec_name);
        if (it != sections_.end()) {
            input_section = &it->second;
        } else {
            input_section = nullptr;
        }
    } else if (elf_struct.st_shndx >= SHN_LORESERVE) {
        input_section = nullptr;
    } else {
        continue;
    }
    
    Symbol<E> new_symbol{
        .name = name,
        .input_section = input_section,
        .output_section = nullptr,
        .is_weak = (elf_struct.st_bind() == elf::STB_WEAK ||
                    elf_struct.st_bind() == elf::STB_GNU_UNIQUE),
        .addr = elf_struct.st_value
    };
    
    Symbol<E> existing;
    if (!ctx.symbol_map.get(name, existing) || !existing.is_defined()) {
        ctx.symbol_map.insert(name, new_symbol);
        continue;
    }
    
    if (!new_symbol.is_defined()) {
        continue;
    }
    
    if (existing.is_weak && !new_symbol.is_weak) {
        ctx.symbol_map.insert(name, new_symbol);
    }
  }

  std::vector<std::future<void>> futures;
  std::mutex local_mutex;

  for (elf::Sym<E>& elf_struct : local_symtab_) {
    if (elf_struct.st_type() == elf::STT_FILE) continue;
    if (elf_struct.st_type() == elf::STT_SECTION) continue;
    if (elf_struct.st_name == 0) continue;
    
    futures.push_back(ctx.thread_pool.submit([&, elf_struct]() {
      const char* name_ptr = strtab_ + elf_struct.st_name;
      std::string name(name_ptr);
      
      InputSection<E>* input_section = nullptr;
      
      if (elf_struct.st_shndx == elf::SHN_UNDEF) {
        input_section = nullptr;
      } else if (elf_struct.st_shndx < shdr_tab_.size()) {
        size_t idx = elf_struct.st_shndx;
        const char* sec_name = shstrtab_ + shdr_tab_[idx].sh_name;
        auto it = sections_.find(sec_name);
        if (it != sections_.end()) {
          input_section = &it->second;
        } else {
          input_section = nullptr;
        }
      } else if (elf_struct.st_shndx >= SHN_LORESERVE) {
        input_section = nullptr;
      } else {
        return;
      }
      
      std::lock_guard<std::mutex> lock(local_mutex);
      ctx.local_symbols.push_back(Symbol<E>{
        .name = name,
        .input_section = input_section,
        .output_section = nullptr,
        .is_weak = (elf_struct.st_bind() == elf::STB_WEAK ||
                    elf_struct.st_bind() == elf::STB_GNU_UNIQUE),
        .addr = elf_struct.st_value
      });
    }));
  }
  
  for (auto& f : futures) {
    f.get();
  }
}

template <typename E>
void ObjectFile<E>::merge_sections(Context<E>& ctx) {
  for (auto& [name, input] : sections_) {
    if (!ctx.merged_sections.contains(name)) {
      ctx.merged_sections.insert(name, MergedSection<E>{});
    }

    MergedSection<E>& merged = ctx.merged_sections.at(name);

    input.offset = merged.data.size();
    merged.data.insert(merged.data.end(), input.data.begin(), input.data.end());

    for (elf::Rel<E> rel : input.rel_tab) {
      auto ind = rel.r_sym();

      if (ind >= local_symtab_.size() + non_local_symtab_.size()) {
        Warn().println("invalid symbol index {} in relocation", ind);
        continue;
      }

      auto& elf_struct = ind < local_symtab_.size()
                             ? local_symtab_[ind]
                             : non_local_symtab_[ind - local_symtab_.size()];
      std::string symbol_name;
      if (elf_struct.st_type() == elf::STT_SECTION &&
          elf_struct.st_shndx < shdr_tab_.size()) {
        symbol_name = shstrtab_ + shdr_tab_[elf_struct.st_shndx].sh_name;
      } else {
        symbol_name = strtab_ + elf_struct.st_name;
      }

      rel.r_offset += input.offset;
      merged.relocations.push_back({
          .rel = rel,
          .symbol_name = symbol_name,
      });
    }

    if (input.align > merged.align) {
      merged.align = input.align;
    }
  }
}

template <typename E>
bool ObjectFile<E>::has_non_local(std::string_view str) {
  for (elf::Sym<E>& elf_struct : non_local_symtab_) {
    auto name = strtab_ + elf_struct.st_name;
    if (std::string_view(name) == str)
      return true;
  }
  return false;
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
