#include "src/arch.h"
#include "src/elf.h"
#include "weld.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <print>
#include <sys/stat.h>
#include <vector>

constexpr auto start_addr = 0x400000;
constexpr auto start_offset = 0x1000;

namespace weld {
template <typename E>
// TODO: add -fpie support
void OutputFile<E>::resolve_relocations(Context<E>& ctx) {
  size_t cur_addr = start_addr;

  for (auto& [name, merged] : ctx.merged_sections) {
    cur_addr = align_addr(cur_addr, merged.align);
    ctx.output_sections[name] = OutputSection<E>{
        .data = merged.data,
        .addr = cur_addr,
    };
    cur_addr += merged.data.size();
  }

  for (auto& [name, sym] : ctx.symbol_map) {
    if (sym.is_defined()) {
      sym.output_section = &ctx.output_sections[sym.input_section->name];
      sym.addr += sym.output_section->addr + sym.input_section->offset;
      std::println("section: {}, symbol: {}, addr: {:X}",
                   sym.input_section->name, name, sym.addr);
    }
  }

  for (auto& [name, sec] : ctx.merged_sections) {
    for (Relocation<E>& rel : sec.relocations) {
      auto S = ctx.symbol_map[rel.symbol_name].addr;
      auto P = ctx.output_sections[name].addr + rel.rela.r_offset;
      auto A = rel.rela.r_addend;

      constexpr auto R_X86_64_64 = 1;
      constexpr auto R_X86_64_PC32 = 2;

      auto type = rel.rela.r_info & 0xffffffffL; // FIXME
      if (type == R_X86_64_64) {
        auto result = S + A;
        auto size = 8;
        std::memcpy(sec.data.data() + rel.rela.r_offset, &result, size);
      } else if (type == R_X86_64_PC32) {
        auto result = S + A - P;
        auto size = 4;
        std::memcpy(sec.data.data() + rel.rela.r_offset, &result, size);
      }
    }
  }
}

template <typename E>
void OutputFile<E>::write(Context<E>& ctx, const std::filesystem::path& path) {
  std::vector<u8> sections_data;
  for (auto& [name, section] : ctx.output_sections) {
    sections_data.insert(sections_data.end(), section.data.begin(),
                         section.data.end());
  }

  elf::Ehdr<E> ehdr = {};
  memcpy(ehdr.e_ident,
         "\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00",
         16);
  ehdr.e_type = elf::ET_EXEC;
  ehdr.e_machine = 62;
  ehdr.e_version = 1;
  ehdr.e_entry = ctx.symbol_map["_start"].addr;
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
  phdr.p_vaddr = ctx.output_sections[".text"].addr;
  phdr.p_paddr = ctx.output_sections[".text"].addr;
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
