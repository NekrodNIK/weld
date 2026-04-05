#include "elf.h"
#include "src/arch.h"
#include "weld.h"
#include <cassert>
#include <memory>
#include <print>
#include <span>
#include <utility>

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
  std::span<elf::Shdr<E>> shdr_tab;

  if (auto result = elf::get_shdr_table<E>(this->mapped_.data())) {
    shdr_tab = result.value();
  } else {
    Fatal() << this->mapped_ << " invalid elf";
  }

  auto symtab_hdr = elf::find_shdr<E>(shdr_tab, elf::SHT_SYMTAB);
  // auto strtab_hdr = &shdr_tab[symtab_hdr->sh_link];
  // strtab = reinterpret_cast<char*>(mapped_.raw() + strtab_hdr->sh_offset);

  if (symtab_hdr) {
    if (auto result =
            elf::get_symbols_symtab_or_dynsym<E>(this->mapped_.raw(), *symtab_hdr)) {
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
    if (auto result =
            elf::get_symbols_symtab_or_dynsym<E>(this->mapped_.raw(), *dynsym_hdr)) {
      auto [local, global] = result.value();
      elf_local_symbols_ = local;
      elf_global_symbols_ = global;
    } else {
      Fatal() << std::format("[{}] invalid .dynsym section", this->filename());
    }
  }

  // TODO: symbol resolution
}

// template <typename E>
// void ObjectFile<E>::symbol_resolution(Context<E>& ctx) {
//   for (const elf::Sym<E>& elf_sym : elf_global_symbols_) {
//     auto name = reinterpret_cast<const char*>(strtab + elf_sym.st_name);
//     std::println("{}", name);

//     if (ctx.symbol_map.contains(name)) {
//       if (ctx.symbol_map[name].esym->st_shndx == elf::SHN_UNDEF) {
//         ctx.symbol_map[name].esym = &elf_sym;
//       } else {
//         if (ctx.symbol_map[name].esym->st_info & elf::STB_WEAK) {
//           if (!(elf_sym.st_info & elf::STB_WEAK)) {
//             ctx.symbol_map[name].esym = &elf_sym;
//           }
//         } else {
//           if (!(elf_sym.st_info & elf::STB_WEAK)) {
//             Fatal() << "duplicate definition"; // FIXME
//           }
//         }
//       }
//     } else {
//       ctx.symbol_map[name] = Symbol<E>{.esym = &elf_sym, .name = name};
//     }
//   }
// }

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
