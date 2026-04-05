#include "src/arch.h"
#include "src/elf.h"
#include "weld.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <vector>

namespace {
size_t align_addr(size_t addr, size_t align) {
  return (addr + align - 1) & ~(align - 1);
}
}; // namespace

namespace weld {
template <typename E>
// TODO: add -fpie support
void OutputFile<E>::resolve_relocations(Context<E>& ctx) {
  auto cur_addr = 0x400000;

  for (auto& name : {".text", ".rodata", ".data", ".bss"}) {
    MergedSection<E>& sec = ctx.merged_sections[name]; // FIXME
    cur_addr = align_addr(cur_addr, sec.alignment);
    sec.addr = cur_addr;
    cur_addr += sec.data.size();
  }

  for (auto& [name, symbol] : ctx.symbol_map) {
    symbol.esym->st_value += symbol.section->addr;
  }

  for (auto& [name, sec] : ctx.merged_sections) {
    for (Relocation<E>& rel : sec.relocations) {
      auto S = ctx.symbol_map[rel.symbol_name].esym->st_value;
      auto P = sec.addr + rel.elf_rela->r_offset;
      auto A = rel.elf_rela->r_addend;

      constexpr auto R_X86_64_64 = 1;
      constexpr auto R_X86_64_PC32 = 2;

      switch (rel.elf_rela->r_info & 0xffffffffL) { // FIXME
      case R_X86_64_64:
        *reinterpret_cast<u64*>(sec.data.data() + rel.elf_rela->r_offset) =
            S + A;
        break;
      case R_X86_64_PC32:
        *reinterpret_cast<u32*>(sec.data.data() + rel.elf_rela->r_offset) =
            S + A - P;
        break;
      }
    }
  }
}

template <typename E>
void OutputFile<E>::write(Context<E>& ctx, const std::filesystem::path& path) {
  std::vector<u8> buf;
  elf::Ehdr<E> ehdr = {};
  ehdr.e_ident[0] = 0x7f;
  ehdr.e_ident[1] = 'E';
  ehdr.e_ident[2] = 'L';
  ehdr.e_ident[3] = 'F';
  ehdr.e_ident[4] = E::is_64 ? 2 : 1;
  ehdr.e_ident[5] = 1; // little-endian
  ehdr.e_ident[6] = 1; // ELF version
  ehdr.e_type = elf::ET_EXEC;
  ehdr.e_machine = 62;
  ehdr.e_version = 1;
  ehdr.e_entry = 0x400000;
  ehdr.e_phoff = sizeof(ehdr);
  ehdr.e_phentsize = sizeof(elf::Phdr<E>);
  ehdr.e_phnum = 1;

  elf::Phdr<E> phdr = {};
  phdr.p_type = elf::PT_LOAD;
  phdr.p_flags = elf::PF_R | elf::PF_X;
  phdr.p_align = 0x1000;

  std::vector<u8> sections_data;
  for (auto& name : {".text", ".rodata", ".data", ".bss"}) {
    if (ctx.merged_sections.contains(name)) {
      auto& sec = ctx.merged_sections[name];
      sections_data.insert(sections_data.end(), sec.data.begin(),
                           sec.data.end());
    }
  }

  phdr.p_offset = ehdr.e_phoff + sizeof(phdr);
  phdr.p_vaddr = 0x400000;
  phdr.p_paddr = 0x400000;
  phdr.p_filesz = sections_data.size();
  phdr.p_memsz = sections_data.size();

  buf.resize(sizeof(ehdr) + sizeof(phdr));

  memcpy(buf.data(), &ehdr, sizeof(ehdr));
  memcpy(buf.data() + sizeof(ehdr), &phdr, sizeof(phdr));
  buf.insert(buf.end(), sections_data.begin(), sections_data.end());

  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<char*>(buf.data()), buf.size());
  chmod(path.c_str(), 0755); // FIXME: posix api
}

template class OutputFile<arch::x86_64>;
template class OutputFile<arch::i386>;
}; // namespace weld
