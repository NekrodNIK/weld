#include "elf.h"
#include "src/arch.h"
#include "weld.h"
#include <cassert>
#include <memory>
#include <print>
#include <span>
#include <utility>

namespace weld {

InputFile::InputFile(MappedFile&& mapped)
    : mapped_(std::forward<MappedFile>(mapped)) {}

// TODO: make it easier to print filename
std::unique_ptr<InputFile> InputFile::parse(MappedFile&& mapped) {
  if (!elf::is_elf(mapped.data())) {
    Fatal() << std::format("[{}] file is not elf", mapped.filename());
  }

  // TODO: Add check for architecture passed via cli
  auto arch = elf::get_arch(mapped.data());
  if (arch == arch::Enum::unsupported) {
    Fatal() << std::format("[{}] architecture is not supported",
                           mapped.filename());
  }

  auto create = [&mapped]<typename E>() -> std::unique_ptr<InputFile> {
    auto type = reinterpret_cast<elf::Ehdr<E>*>(mapped.raw())->e_type;
    switch (type) {
    case elf::ET_REL:
      return std::make_unique<ObjectFile<E>>(std::forward<MappedFile>(mapped));
    case elf::ET_DYN:
      return std::make_unique<SharedObjectFile<E>>(
          std::forward<MappedFile>(mapped));
    case elf::ET_EXEC:
      Fatal() << std::format("[{}] is executable", mapped.filename());
    default:
      Fatal() << std::format("[{}] unknown subtype of elf", mapped.filename());
    }
  };

  switch (arch) {
  case arch::Enum::i386:
    return create.operator()<arch::i386>();
  case arch::Enum::x86_64:
    return create.operator()<arch::x86_64>();
  case arch::Enum::unsupported:
  default:
    Fatal() << std::format("[{}] unsupported architecture", mapped.filename());
  }
}

template <typename E>
ObjectFile<E>::ObjectFile(MappedFile&& mapped)
    : InputFile(std::forward<MappedFile>(mapped)) {
  std::span<elf::Shdr<E>> shdr_tab;
  if (auto result = elf::get_shdr_table<E>(mapped_.data())) {
    shdr_tab = result.value();
  } else {
    Fatal() << std::format("[{}] invalid elf", mapped.filename());
  }

  auto symtab_hdr = elf::find_shdr<E>(shdr_tab, elf::SHT_SYMTAB);
  auto strtab_hdr = &shdr_tab[symtab_hdr->sh_link];
  strtab = reinterpret_cast<char*>(mapped_.raw() + strtab_hdr->sh_offset);

  if (symtab_hdr) {
    if (auto result =
            elf::get_symbols_symtab_or_dynsym<E>(mapped_.raw(), *symtab_hdr)) {
      auto [local, global] = result.value();
      elf_local_symbols_ = local;
      elf_global_symbols_ = global;
    } else {
      Fatal() << std::format("[{}] invalid .symtab section", filename());
    }
  }
}

template <typename E>
SharedObjectFile<E>::SharedObjectFile(MappedFile&& mapped)
    : InputFile(std::forward<MappedFile>(mapped)) {
  std::span<elf::Shdr<E>> shdr_tab;
  if (auto result = elf::get_shdr_table<E>(mapped_.data())) {
    shdr_tab = result.value();
  } else {
    Fatal() << std::format("[{}] invalid elf", mapped.filename());
  }

  auto dynsym_hdr = elf::find_shdr<E>(shdr_tab, elf::SHT_DYNSYM);

  if (dynsym_hdr) {
    if (auto result =
            elf::get_symbols_symtab_or_dynsym<E>(mapped_.raw(), *dynsym_hdr)) {
      auto [local, global] = result.value();
      elf_local_symbols_ = local;
      elf_global_symbols_ = global;
    } else {
      Fatal() << std::format("[{}] invalid .dynsym section", filename());
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

template class ObjectFile<arch::i386>;
template class ObjectFile<arch::x86_64>;
} // namespace weld
