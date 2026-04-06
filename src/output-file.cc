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
  auto cur_off = 0x1000;

  for (auto& name : {".text", ".rodata", ".data", ".bss"}) {
    MergedSection<E>& sec = ctx.merged_sections[name]; // FIXME
    cur_addr = align_addr(cur_addr, sec.alignment);
    cur_off = align_addr(cur_off, sec.alignment);
    sec.addr = cur_addr;
    sec.file_off = cur_off;
    cur_addr += sec.data.size();
    cur_off += sec.data.size();
  }

  for (auto& [name, sym] : ctx.symbol_map) {
    if (sym.section && sym.input_section) {
        sym.esym->st_value = sym.section->addr + sym.input_section->offset + sym.esym->st_value;
    }
    if (name == "main" || name == "_start" || name == "foo") {
      printf("%s: st_value=0x%lx, section=%p, section->addr=0x%lx\n",
             name.c_str(), sym.esym->st_value, sym.section,
             sym.section ? sym.section->addr : 0);
    }
  }

  for (auto& [name, sec] : ctx.merged_sections) {
    for (Relocation<E>& rel : sec.relocations) {
      auto S = ctx.symbol_map[rel.symbol_name].esym->st_value;
      auto P = sec.addr + rel.elf_rela.r_offset;
      auto A = rel.elf_rela.r_addend;

      constexpr auto R_X86_64_64 = 1;
      constexpr auto R_X86_64_PC32 = 2;

      switch (rel.elf_rela.r_info & 0xffffffffL) { // FIXME
      case R_X86_64_64:
        *reinterpret_cast<u64*>(sec.data.data() + rel.elf_rela.r_offset) =
            S + A;
        break;
      case R_X86_64_PC32:
        *reinterpret_cast<u32*>(sec.data.data() + rel.elf_rela.r_offset) =
            S + A - P;
        break;
      }
    }
  }
}

template <typename E>
void OutputFile<E>::write(Context<E>& ctx, const std::filesystem::path& path) {
  std::vector<u8> sections_data;
  for (auto& name : {".text", ".rodata", ".data"}) {
    if (ctx.merged_sections.contains(name)) {
      auto& sec = ctx.merged_sections[name];
      sections_data.insert(sections_data.end(), sec.data.begin(),
                           sec.data.end());
    }
  }

  elf::Ehdr<E> ehdr = {};
  memcpy(ehdr.e_ident,
         "\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00",
         16);
  ehdr.e_type = elf::ET_EXEC;
  ehdr.e_machine = 62;
  ehdr.e_version = 1;
  ehdr.e_entry = ctx.symbol_map["_start"].esym->st_value;
  ehdr.e_phoff = sizeof(ehdr);
  ehdr.e_phentsize = sizeof(elf::Phdr<E>);
  ehdr.e_phnum = 1;
  ehdr.e_shoff = 0;
  ehdr.e_shnum = 0;
  ehdr.e_shstrndx = 0;

  elf::Phdr<E> phdr = {};
  phdr.p_type = elf::PT_LOAD;
  phdr.p_flags = elf::PF_R | elf::PF_X;
  phdr.p_align = 0x1000;

  size_t text_offset = align_addr(sizeof(ehdr) + sizeof(phdr), 0x1000);
  phdr.p_offset = text_offset;
  phdr.p_vaddr = ctx.merged_sections[".text"].addr;
  phdr.p_paddr = ctx.merged_sections[".text"].addr;
  phdr.p_filesz = sections_data.size();
  phdr.p_memsz = sections_data.size();

  std::vector<u8> buf(text_offset);
  memcpy(buf.data(), &ehdr, sizeof(ehdr));
  memcpy(buf.data() + sizeof(ehdr), &phdr, sizeof(phdr));
  buf.insert(buf.end(), sections_data.begin(), sections_data.end());

  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<char*>(buf.data()), buf.size());
  chmod(path.c_str(), 0755);
}

template class OutputFile<arch::x86_64>;
template class OutputFile<arch::i386>;
}; // namespace weld
