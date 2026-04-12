#include "arch.h"
#include "elf.h"
#include "weld.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

constexpr auto start_addr = 0x400000;

namespace weld {
template <typename E>
// TODO: add -fpie support
// TODO: refactoring
void OutputFile<E>::resolve_relocations(Context<E>& ctx) {
  size_t cur_addr = start_addr;
  std::unordered_map<std::string, size_t> output_sec_ind;

  auto generate_output_sec = [&ctx, &cur_addr, &output_sec_ind](
                                 MergedSection<E> merged, std::string name) {
    cur_addr = align_addr(cur_addr, merged.align);
    ctx.output_sections.push_back({
        .name = name,
        .data = std::move(merged.data),
        .addr = cur_addr,
        .relocations = std::move(merged.relocations),
    });
    output_sec_ind[name] = ctx.output_sections.size() - 1;
    cur_addr += ctx.output_sections.back().data.size();
  };

  for (auto& [name, merged] : ctx.merged_sections) {
    if (name.find(".text") != 0)
      continue;
    generate_output_sec(merged, name);
  }

  cur_addr = align_addr(cur_addr, 0x1000);
  for (auto& [name, merged] : ctx.merged_sections) {
    if (name.find(".rodata") != 0)
      continue;
    generate_output_sec(merged, name);
  }

  cur_addr = align_addr(cur_addr, 0x1000);
  for (auto& [name, merged] : ctx.merged_sections) {
    if (name.find(".text") == 0 || name.find(".rodata") == 0)
      continue;
    generate_output_sec(merged, name);
  }

  auto set_addr = [&ctx, &output_sec_ind](auto& sym) {
    if (sym.is_defined()) {
      sym.output_section =
          &ctx.output_sections[output_sec_ind[sym.input_section->name]];
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

  for (auto& sec : ctx.output_sections) {
    for (Relocation<E>& rel : sec.relocations) {
      auto S = ctx.symbol_map[rel.symbol_name].addr;
      auto P = ctx.output_sections[output_sec_ind[sec.name]].addr +
               rel.rela.r_offset;
      auto A = rel.rela.r_addend;

      constexpr auto R_X86_64_64 = 1;
      constexpr auto R_X86_64_PC32 = 2;
      constexpr auto R_X86_64_PLT32 = 4;
      constexpr auto R_X86_64_32 = 10;
      constexpr auto R_X86_64_32S = 11;

      auto type = rel.rela.r_info & 0xffffffffL;
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
  std::vector<u8> text_bytes;
  std::vector<u8> rodata_bytes;
  std::vector<u8> data_bytes;
  size_t bss_size = 0;

  for (auto& section : ctx.output_sections) {
    if (section.name.find(".bss") == 0) {
      bss_size += section.data.size();
    } else if (section.name.find(".text") == 0) {
      text_bytes.insert(text_bytes.end(), section.data.begin(),
                        section.data.end());
    } else if (section.name.find(".rodata") == 0) {
      rodata_bytes.insert(rodata_bytes.end(), section.data.begin(),
                          section.data.end());
    } else {
      data_bytes.insert(data_bytes.end(), section.data.begin(),
                        section.data.end());
    }
  }

  size_t headers_size = sizeof(elf::Ehdr<E>) + 3 * sizeof(elf::Phdr<E>);

  size_t text_offset = align_addr(headers_size, 0x1000);
  size_t text_vaddr = start_addr;

  size_t rodata_offset = align_addr(text_offset + text_bytes.size(), 0x1000);
  size_t rodata_vaddr = align_addr(text_vaddr + text_bytes.size(), 0x1000);

  size_t data_offset = align_addr(rodata_offset + rodata_bytes.size(), 0x1000);
  size_t data_vaddr = align_addr(rodata_vaddr + rodata_bytes.size(), 0x1000);

  std::vector<elf::Phdr<E>> phdrs;

  if (!text_bytes.empty()) {
    elf::Phdr<E> phdr_text;
    phdr_text.p_type = elf::PT_LOAD;
    phdr_text.p_flags = elf::PF_R | elf::PF_X;
    phdr_text.p_align = 0x1000;
    phdr_text.p_offset = text_offset;
    phdr_text.p_vaddr = text_vaddr;
    phdr_text.p_paddr = text_vaddr;
    phdr_text.p_filesz = text_bytes.size();
    phdr_text.p_memsz = text_bytes.size();
    phdrs.push_back(phdr_text);
  }

  if (!rodata_bytes.empty()) {
    elf::Phdr<E> phdr_rodata;
    phdr_rodata.p_type = elf::PT_LOAD;
    phdr_rodata.p_flags = elf::PF_R;
    phdr_rodata.p_align = 0x1000;
    phdr_rodata.p_offset = rodata_offset;
    phdr_rodata.p_vaddr = rodata_vaddr;
    phdr_rodata.p_paddr = rodata_vaddr;
    phdr_rodata.p_filesz = rodata_bytes.size();
    phdr_rodata.p_memsz = rodata_bytes.size();
    phdrs.push_back(phdr_rodata);
  }

  if (!data_bytes.empty() || bss_size > 0) {
    elf::Phdr<E> phdr_data;
    phdr_data.p_type = elf::PT_LOAD;
    phdr_data.p_flags = elf::PF_R | elf::PF_W;
    phdr_data.p_align = 0x1000;
    phdr_data.p_offset = data_offset;
    phdr_data.p_vaddr = data_vaddr;
    phdr_data.p_paddr = data_vaddr;
    phdr_data.p_filesz = data_bytes.size();
    phdr_data.p_memsz = data_bytes.size() + bss_size;
    phdrs.push_back(phdr_data);
  }

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

  if (!text_bytes.empty())
    process_section(".text", elf::SHT_PROGBITS,
                    elf::SHF_ALLOC | elf::SHF_EXECINSTR, text_offset,
                    text_bytes.size(), text_vaddr);
  if (!rodata_bytes.empty())
    process_section(".rodata", elf::SHT_PROGBITS, elf::SHF_ALLOC, rodata_offset,
                    rodata_bytes.size(), rodata_vaddr);
  if (!data_bytes.empty())
    process_section(".data", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_WRITE,
                    data_offset, data_bytes.size(), data_vaddr);
  if (bss_size > 0)
    process_section(".bss", elf::SHT_NOBITS, elf::SHF_ALLOC | elf::SHF_WRITE, 0,
                    bss_size, data_vaddr + data_bytes.size());

  size_t symtab_offset = align_addr(data_offset + data_bytes.size(), 0x1000);
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
  ehdr.e_phnum = phdrs.size();
  ehdr.e_shoff = shdr_tab_offset;
  ehdr.e_shentsize = sizeof(elf::Shdr<E>);
  ehdr.e_shnum = shdr_tab.size();
  ehdr.e_shstrndx = shstr_idx;

  size_t total_size = shstrtab_offset + shstrtab.size();
  std::vector<u8> buf(total_size);

  memcpy(buf.data(), &ehdr, sizeof(ehdr));
  for (size_t i = 0; i < phdrs.size(); i++) {
    memcpy(buf.data() + sizeof(ehdr) + i * sizeof(elf::Phdr<E>), &phdrs[i],
           sizeof(elf::Phdr<E>));
  }

  if (!text_bytes.empty())
    memcpy(buf.data() + text_offset, text_bytes.data(), text_bytes.size());
  if (!rodata_bytes.empty())
    memcpy(buf.data() + rodata_offset, rodata_bytes.data(),
           rodata_bytes.size());
  if (!data_bytes.empty())
    memcpy(buf.data() + data_offset, data_bytes.data(), data_bytes.size());
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
} // namespace weld
