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

  auto set_addr = [&ctx](auto& sym) {
    if (sym.is_defined()) {
      sym.output_section = &ctx.output_sections[sym.input_section->name];
      sym.addr += sym.output_section->addr + sym.input_section->offset;
      std::println("section: {}, symbol: {}, addr: {:X}",
                   sym.input_section->name, sym.name, sym.addr);
    }
  };
  
  for (auto& [_, sym] : ctx.symbol_map) {
    set_addr(sym);
  }
  for (auto& sym : ctx.local_symbols) {
    set_addr(sym);
  }

  for (auto& [name, sec] : ctx.merged_sections) {
    for (Relocation<E>& rel : sec.relocations) {
      auto S = ctx.symbol_map[rel.symbol_name].addr;
      auto P = ctx.output_sections[name].addr + rel.rela.r_offset;
      auto A = rel.rela.r_addend;

      constexpr auto R_X86_64_64 = 1;
      constexpr auto R_X86_64_PC32 = 2;
      constexpr auto R_X86_64_PLT32 = 4;
      constexpr auto R_X86_64_32 = 10;
      constexpr auto R_X86_64_32S = 11;

      auto type = rel.rela.r_info & 0xffffffffL; // FIXME
      if (type == R_X86_64_64) {
        auto result = S + A;
        auto size = 8;
        std::memcpy(sec.data.data() + rel.rela.r_offset, &result, size);
      } else if (type == R_X86_64_PC32 ||
                 type == R_X86_64_PLT32) { // FIXME: plt stub
        auto result = S + A - P;
        auto size = 4;
        std::memcpy(sec.data.data() + rel.rela.r_offset, &result, size);
      } else if (type == R_X86_64_32 || type == R_X86_64_32S) {
        auto result = S + A;
        auto size = 4;
        std::memcpy(sec.data.data() + rel.rela.r_offset, &result, size);
      } else {
        Fatal().println("unknown relocation type: {}", type);
      }
    }
  }
}

template <typename E>
void OutputFile<E>::write(Context<E>& ctx, const std::filesystem::path& path) {
  std::vector<u8> re_bytes;
  std::vector<u8> rw_bytes;
  size_t bss_size = 0;

  for (auto& [name, section] : ctx.output_sections) {
    if (name.find(".bss") == 0) {
      bss_size += section.data.size();
    } else if (name.find(".text") == 0 || name.find(".rodata") == 0) {
      re_bytes.insert(re_bytes.end(), section.data.begin(), section.data.end());
    } else {
      rw_bytes.insert(rw_bytes.end(), section.data.begin(), section.data.end());
    }
  }

  elf::Ehdr<E> ehdr;
  memcpy(ehdr.e_ident,
         "\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00",
         16);
  ehdr.e_type = elf::ET_EXEC;
  ehdr.e_machine = 62;
  ehdr.e_version = 1;
  ehdr.e_entry = ctx.symbol_map["_start"].addr;
  ehdr.e_phoff = sizeof(ehdr);
  ehdr.e_phentsize = sizeof(elf::Phdr<E>);
  ehdr.e_phnum = 2;
  ehdr.e_shoff = 0;
  ehdr.e_shnum = 0;
  ehdr.e_shstrndx = 0;

  size_t headers_size = sizeof(ehdr) + 2 * sizeof(elf::Phdr<E>);
  size_t re_offset = align_addr(headers_size, 0x1000);
  size_t re_vaddr = start_addr;

  size_t rw_offset = align_addr(re_offset + re_bytes.size(), 0x1000);
  size_t rw_vaddr = align_addr(re_vaddr + re_bytes.size(), 0x1000);

  elf::Phdr<E> phdr_re;
  phdr_re.p_type = elf::PT_LOAD;
  phdr_re.p_flags = elf::PF_R | elf::PF_X;
  phdr_re.p_align = 0x1000;
  phdr_re.p_offset = re_offset;
  phdr_re.p_vaddr = re_vaddr;
  phdr_re.p_paddr = re_vaddr;
  phdr_re.p_filesz = re_bytes.size();
  phdr_re.p_memsz = re_bytes.size();

  elf::Phdr<E> phdr_rw;
  phdr_rw.p_type = elf::PT_LOAD;
  phdr_rw.p_flags = elf::PF_R | elf::PF_W;
  phdr_rw.p_align = 0x1000;
  phdr_rw.p_offset = rw_offset;
  phdr_rw.p_vaddr = rw_vaddr;
  phdr_rw.p_paddr = rw_vaddr;
  phdr_rw.p_filesz = rw_bytes.size();
  phdr_rw.p_memsz = rw_bytes.size() + bss_size;

  std::vector<u8> buf(rw_offset + rw_bytes.size());

  memcpy(buf.data(), &ehdr, sizeof(ehdr));
  memcpy(buf.data() + sizeof(ehdr), &phdr_re, sizeof(phdr_re));
  memcpy(buf.data() + sizeof(ehdr) + sizeof(phdr_re), &phdr_rw,
         sizeof(phdr_rw));
  if (!re_bytes.empty()) {
    memcpy(buf.data() + re_offset, re_bytes.data(), re_bytes.size());
  }
  if (!rw_bytes.empty()) {
    memcpy(buf.data() + rw_offset, rw_bytes.data(), rw_bytes.size());
  }

  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<char*>(buf.data()), buf.size());
  chmod(path.c_str(), 0755);
}

template class OutputFile<arch::x86_64>;
template class OutputFile<arch::i386>;
}; // namespace weld
