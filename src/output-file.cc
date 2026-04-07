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
#include <unordered_map>
#include <vector>

constexpr auto start_addr = 0x400000;

namespace weld {
template <typename E>
// TODO: add -fpie support
void OutputFile<E>::resolve_relocations(Context<E>& ctx) {
  size_t cur_addr = start_addr;

  static std::unordered_map<std::string, OutputSection<E>*> name_to_section;

  for (auto& [name, merged] : ctx.merged_sections) {
    cur_addr = align_addr(cur_addr, merged.align);
    ctx.output_sections.push_back({.name = name, .data = merged.data, .addr = cur_addr});
    name_to_section[name] = &ctx.output_sections.back();
    cur_addr += merged.data.size();
  }

  auto set_addr = [&ctx](auto& sym) {
    if (sym.is_defined()) {
      sym.output_section = name_to_section[sym.input_section->name];
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
      auto P = name_to_section[name]->addr + rel.rela.r_offset;
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

// TODO: refactoring
template <typename E>
void OutputFile<E>::write(Context<E>& ctx, const std::filesystem::path& path) {
  std::vector<u8> re_bytes;
  std::vector<u8> rw_bytes;
  size_t bss_size = 0;

  for (auto& section : ctx.output_sections) {
    if (section.name.find(".bss") == 0) {
      bss_size += section.data.size();
    } else if (section.name.find(".text") == 0 || section.name.find(".rodata") == 0) {
      re_bytes.insert(re_bytes.end(), section.data.begin(), section.data.end());
    } else {
      rw_bytes.insert(rw_bytes.end(), section.data.begin(), section.data.end());
    }
  }

  size_t headers_size = sizeof(elf::Ehdr<E>) + 2 * sizeof(elf::Phdr<E>);
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

  std::vector<elf::Sym<E>> symtab;
  std::vector<char> strtab{'\0'};
  std::vector<char> shstrtab{'\0'};
  std::vector<elf::Shdr<E>> shdr_tab(1);

  auto process_symbol = [&symtab, &strtab](Symbol<E>& symbol) {
    elf::Sym<E> elf_struct = {};
    elf_struct.st_name = strtab.size();
    elf_struct.st_info =
        (symbol.is_weak ? elf::STB_WEAK : elf::STB_GLOBAL) << 4 | elf::STT_FUNC;
    elf_struct.st_other = 0;
    elf_struct.st_shndx = 1;
    elf_struct.st_value = symbol.addr;
    elf_struct.st_size = 0;

    symtab.push_back(elf_struct);

    strtab.resize(strtab.size() + symbol.name.size() + 1);
    memcpy(strtab.data() + elf_struct.st_name, symbol.name.c_str(),
           symbol.name.size() + 1);
  };

  for (auto& symbol : ctx.local_symbols)
    process_symbol(symbol);
  for (auto& [_, symbol] : ctx.symbol_map)
    process_symbol(symbol);

  auto process_section = [&shdr_tab,
                          &shstrtab](const std::string& name, u32 type,
                                     u32 flags, size_t offset, size_t size,
                                     size_t addr, u32 link = 0, u32 info = 0) {
    elf::Shdr<E> shdr = {};
    shdr.sh_name = shstrtab.size();
    shdr.sh_type = type;
    shdr.sh_flags = flags;
    shdr.sh_addr = addr;
    shdr.sh_offset = offset;
    shdr.sh_size = size;
    shdr.sh_link = link;
    shdr.sh_info = info;
    shdr.sh_addralign = 1;
    shdr.sh_entsize = (type == elf::SHT_SYMTAB) ? sizeof(elf::Sym<E>) : 0;

    shdr_tab.push_back(shdr);

    shstrtab.resize(shstrtab.size() + name.size() + 1);
    memcpy(shstrtab.data() + shdr.sh_name, name.c_str(), name.size() + 1);
  };

  process_section(".text", elf::SHT_PROGBITS,
                  elf::SHF_ALLOC | elf::SHF_EXECINSTR, re_offset,
                  re_bytes.size(), re_vaddr);
  process_section(".rodata", elf::SHT_PROGBITS, elf::SHF_ALLOC,
                  re_offset + re_bytes.size(), 0, re_vaddr + re_bytes.size());
  process_section(".data", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_WRITE,
                  rw_offset, rw_bytes.size(), rw_vaddr);
  process_section(".bss", elf::SHT_NOBITS, elf::SHF_ALLOC | elf::SHF_WRITE, 0,
                  bss_size, rw_vaddr + rw_bytes.size());

  size_t symtab_offset = align_addr(rw_offset + rw_bytes.size(), 0x1000);
  size_t sym_idx = shdr_tab.size();
  process_section(".symtab", elf::SHT_SYMTAB, 0, symtab_offset,
                  symtab.size() * sizeof(elf::Sym<E>), 0, 0, 1);

  size_t strtab_offset =
      align_addr(symtab_offset + symtab.size() * sizeof(elf::Sym<E>), 0x1000);
  size_t str_idx = shdr_tab.size();
  process_section(".strtab", elf::SHT_STRTAB, 0, strtab_offset, 0, 0);
  shdr_tab[str_idx].sh_size = strtab.size();

  size_t shdr_tab_offset = align_addr(strtab_offset + strtab.size(), 0x1000);

  size_t shstrtab_offset = align_addr(
      shdr_tab_offset + shdr_tab.size() * sizeof(elf::Shdr<E>), 0x1000);
  size_t shstr_idx = shdr_tab.size();
  process_section(".shstrtab", elf::SHT_STRTAB, 0, shstrtab_offset, 0, 0);
  shdr_tab[shstr_idx].sh_size = shstrtab.size();

  shdr_tab[sym_idx].sh_link = str_idx;
  shdr_tab[sym_idx].sh_info = 1;

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
  ehdr.e_shoff = shdr_tab_offset;
  ehdr.e_shentsize = sizeof(elf::Shdr<E>);
  ehdr.e_shnum = shdr_tab.size();
  ehdr.e_shstrndx = shstr_idx;

  size_t total_size = shstrtab_offset + shstrtab.size();
  std::vector<u8> buf(total_size);

  memcpy(buf.data(), &ehdr, sizeof(ehdr));
  memcpy(buf.data() + sizeof(ehdr), &phdr_re, sizeof(phdr_re));
  memcpy(buf.data() + sizeof(ehdr) + sizeof(phdr_re), &phdr_rw,
         sizeof(phdr_rw));
  if (!re_bytes.empty())
    memcpy(buf.data() + re_offset, re_bytes.data(), re_bytes.size());
  if (!rw_bytes.empty())
    memcpy(buf.data() + rw_offset, rw_bytes.data(), rw_bytes.size());
  if (!symtab.empty())
    memcpy(buf.data() + symtab_offset, symtab.data(),
           symtab.size() * sizeof(elf::Sym<E>));
  if (!strtab.empty())
    memcpy(buf.data() + strtab_offset, strtab.data(), strtab.size());
  if (!shdr_tab.empty())
    memcpy(buf.data() + shdr_tab_offset, shdr_tab.data(),
           shdr_tab.size() * sizeof(elf::Shdr<E>));
  if (!shstrtab.empty())
    memcpy(buf.data() + shstrtab_offset, shstrtab.data(), shstrtab.size());

  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<char*>(buf.data()), buf.size());
  chmod(path.c_str(), 0755);
}

template class OutputFile<arch::x86_64>;
template class OutputFile<arch::i386>;
}; // namespace weld
