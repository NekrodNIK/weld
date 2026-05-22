#include "arch.h"
#include "elf.h"
#include "weld.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <fcntl.h> 
#include <filesystem>
#include <functional>
#include <print>
#include <string>
#include <sys/stat.h>
#include <sys/mman.h>  
#include <unistd.h>
#include <unordered_map>
#include <vector>

constexpr auto start_addr = 0x400000;

namespace weld {
template <typename E>
// TODO: add -fpie support
// TODO: refactoring
void OutputFile<E>::resolve_relocations(Context<E>& ctx) {
  size_t cur_addr = start_addr;

  auto generate_output_sec = [&ctx, &cur_addr](MergedSection<E> merged,
                                               std::string name) {
    cur_addr = align_addr(cur_addr, merged.align);
    ctx.output_sections.push_back({
        .name = name,
        .data = std::move(merged.data),
        .addr = cur_addr,
        .relocations = std::move(merged.relocations),
    });
    ctx.output_sec_ind.insert(name, ctx.output_sections.size() - 1);
    cur_addr += ctx.output_sections.back().data.size();
  };

  for (const auto& [name, merged] : ctx.merged_sections) {
    if (name.find(".text") != 0)
      continue;
    generate_output_sec(merged, name);
  }

  cur_addr = align_addr(cur_addr, 0x1000);
  for (const auto& [name, merged] : ctx.merged_sections) {
    if (name.find(".rodata") != 0)
      continue;
    generate_output_sec(merged, name);
  }

  cur_addr = align_addr(cur_addr, 0x1000);
  for (const auto& [name, merged] : ctx.merged_sections) {
    if (name.find(".text") == 0 || name.find(".rodata") == 0)
      continue;
    generate_output_sec(merged, name);
  }

  auto set_addr = [&ctx](auto& sym) {
    if (sym.is_defined()) {
      sym.output_section =
          &ctx.output_sections[ctx.output_sec_ind.at(sym.input_section->name)];
      sym.addr += sym.output_section->addr + sym.input_section->offset;
      std::println("section: {}, symbol: {}, addr: {:X}",
                   sym.input_section->name, sym.name, sym.addr);
    }
  };

  for (const auto& [_, sym] : ctx.symbol_map) {
    set_addr(sym);
  }
  for (auto& sym : ctx.local_symbols) {
    set_addr(sym);
  }

  for (auto& sec : ctx.output_sections) {
    for (Relocation<E>& rel : sec.relocations) {
      auto S = ctx.symbol_map[rel.symbol_name].addr;
      auto P = ctx.output_sections[ctx.output_sec_ind[sec.name]].addr +
               rel.rel.r_offset;
      auto A = rel.rel.r_addend;

      constexpr auto R_X86_64_64 = 1;
      constexpr auto R_X86_64_PC32 = 2;
      constexpr auto R_X86_64_PLT32 = 4;
      constexpr auto R_X86_64_32 = 10;
      constexpr auto R_X86_64_32S = 11;

      auto type = rel.rel.r_info & 0xffffffffL;
      if (type == R_X86_64_64) {
        auto result = S + A;
        auto size = 8;
        std::memcpy(sec.data.data() + rel.rel.r_offset, &result, size);
      } else if (type == R_X86_64_PC32 ||
                 type == R_X86_64_PLT32) { // FIXME: plt stub
        auto result = S + A - P;
        auto size = 4;
        std::memcpy(sec.data.data() + rel.rel.r_offset, &result, size);
      } else if (type == R_X86_64_32 || type == R_X86_64_32S) {
        auto result = S + A;
        auto size = 4;
        std::memcpy(sec.data.data() + rel.rel.r_offset, &result, size);
      } else {
        Fatal().println("unknown relocation type: {}", type);
      }
    }
  }
}

// TODO: refactored?
template <typename E>
void OutputFile<E>::write(Context<E>& ctx, const std::filesystem::path& path) {
    std::vector<u8> text_bytes, rodata_bytes, data_bytes;
    size_t bss_size = 0;
    for (auto& section : ctx.output_sections) {
        if (section.name.find(".bss") == 0)
            bss_size += section.data.size();
        else if (section.name.find(".text") == 0)
            text_bytes.insert(text_bytes.end(), section.data.begin(), section.data.end());
        else if (section.name.find(".rodata") == 0)
            rodata_bytes.insert(rodata_bytes.end(), section.data.begin(), section.data.end());
        else
            data_bytes.insert(data_bytes.end(), section.data.begin(), section.data.end());
    }

    std::vector<elf::Sym<E>> symtab{{}};
    std::vector<char> strtab{'\0'};
    std::vector<char> shstrtab{'\0'};
    std::vector<elf::Shdr<E>> shdr_tab(1);

    auto process_symbol = [&](Symbol<E>& symbol) {
        elf::Sym<E> elf_sym{};
        int shndx = elf::SHN_ABS;
        if (symbol.input_section) {
            const std::string& sec_name = symbol.input_section->name;
            shndx = ctx.output_sec_ind[sec_name] + 1;
        }
        elf_sym.st_name = strtab.size();
        elf_sym.st_info =
            (symbol.is_weak ? elf::STB_WEAK : elf::STB_GLOBAL) << 4 | elf::STT_OBJECT;
        elf_sym.st_value = symbol.addr;
        elf_sym.st_shndx = shndx;
        elf_sym.st_size = 0;
        symtab.push_back(elf_sym);
        strtab.resize(strtab.size() + symbol.name.size() + 1);
        std::memcpy(strtab.data() + elf_sym.st_name, symbol.name.c_str(),
                    symbol.name.size() + 1);
    };

    for (auto& sym : ctx.local_symbols) {
      process_symbol(sym);
    }

    for (auto& [_, sym] : ctx.symbol_map) {
      process_symbol(sym);
    }

    auto add_section_header = [&](const std::string& name, u32 type, u32 flags,
                                  size_t size, size_t addr,
                                  u32 link = 0, u32 info = 0, size_t entsize = 0) {
        elf::Shdr<E> shdr{};
        shdr.sh_name = shstrtab.size();
        shdr.sh_type = type;
        shdr.sh_flags = flags;
        shdr.sh_addr = addr;
        shdr.sh_size = size;
        shdr.sh_link = link;
        shdr.sh_info = info;
        shdr.sh_addralign = 1;
        shdr.sh_entsize = entsize;
        shdr_tab.push_back(shdr);
        shstrtab.resize(shstrtab.size() + name.size() + 1);
        std::memcpy(shstrtab.data() + shdr.sh_name, name.c_str(), name.size() + 1);
        return shdr_tab.size() - 1;
    };

    size_t text_idx = 0, rodata_idx = 0, data_idx = 0, bss_idx = 0;
    if (!text_bytes.empty())
        text_idx = add_section_header(".text", elf::SHT_PROGBITS,
                                      elf::SHF_ALLOC | elf::SHF_EXECINSTR,
                                      text_bytes.size(), 0);
    if (!rodata_bytes.empty())
        rodata_idx = add_section_header(".rodata", elf::SHT_PROGBITS,
                                        elf::SHF_ALLOC, rodata_bytes.size(), 0);
    if (!data_bytes.empty())
        data_idx = add_section_header(".data", elf::SHT_PROGBITS,
                                      elf::SHF_ALLOC | elf::SHF_WRITE,
                                      data_bytes.size(), 0);
    if (bss_size > 0)
        bss_idx = add_section_header(".bss", elf::SHT_NOBITS,
                                     elf::SHF_ALLOC | elf::SHF_WRITE,
                                     bss_size, 0);

    size_t sym_idx = add_section_header(".symtab", elf::SHT_SYMTAB, 0,
                                        symtab.size() * sizeof(elf::Sym<E>), 0,
                                        0, 1, sizeof(elf::Sym<E>));
    size_t str_idx = add_section_header(".strtab", elf::SHT_STRTAB, 0,
                                        strtab.size(), 0);
    size_t shstr_idx = add_section_header(".shstrtab", elf::SHT_STRTAB, 0,
                                          shstrtab.size(), 0);

    shdr_tab[sym_idx].sh_link = str_idx;
    shdr_tab[sym_idx].sh_info = 1;

    const bool is_rel = ctx.is_relocatable;
    const u16 elf_type = is_rel ? elf::ET_REL : elf::ET_EXEC;

    constexpr size_t PAGE = 0x1000;
    size_t headers_size;
    size_t text_offset = 0, rodata_offset = 0, data_offset = 0;
    size_t text_vaddr = 0, rodata_vaddr = 0, data_vaddr = 0;
    std::vector<elf::Phdr<E>> phdrs;

    if (!is_rel) {
        headers_size = sizeof(elf::Ehdr<E>) + 3 * sizeof(elf::Phdr<E>);
        text_offset = align_addr(headers_size, PAGE);
        text_vaddr = ctx.start_addr;
        rodata_offset = align_addr(text_offset + text_bytes.size(), PAGE);
        rodata_vaddr = align_addr(text_vaddr + text_bytes.size(), PAGE);
        data_offset = align_addr(rodata_offset + rodata_bytes.size(), PAGE);
        data_vaddr = align_addr(rodata_vaddr + rodata_bytes.size(), PAGE);

        if (!text_bytes.empty()) {
            elf::Phdr<E> ph{}; ph.p_type = elf::PT_LOAD; ph.p_flags = elf::PF_R | elf::PF_X;
            ph.p_align = PAGE; ph.p_offset = text_offset; ph.p_vaddr = text_vaddr;
            ph.p_paddr = text_vaddr; ph.p_filesz = text_bytes.size(); ph.p_memsz = text_bytes.size();
            phdrs.push_back(ph);
        }
        if (!rodata_bytes.empty()) {
            elf::Phdr<E> ph{}; ph.p_type = elf::PT_LOAD; ph.p_flags = elf::PF_R;
            ph.p_align = PAGE; ph.p_offset = rodata_offset; ph.p_vaddr = rodata_vaddr;
            ph.p_paddr = rodata_vaddr; ph.p_filesz = rodata_bytes.size(); ph.p_memsz = rodata_bytes.size();
            phdrs.push_back(ph);
        }
        if (!data_bytes.empty() || bss_size > 0) {
            elf::Phdr<E> ph{}; ph.p_type = elf::PT_LOAD; ph.p_flags = elf::PF_R | elf::PF_W;
            ph.p_align = PAGE; ph.p_offset = data_offset; ph.p_vaddr = data_vaddr;
            ph.p_paddr = data_vaddr; ph.p_filesz = data_bytes.size();
            ph.p_memsz = data_bytes.size() + bss_size;
            phdrs.push_back(ph);
        }

        if (text_idx) shdr_tab[text_idx].sh_addr = text_vaddr;
        if (rodata_idx) shdr_tab[rodata_idx].sh_addr = rodata_vaddr;
        if (data_idx) shdr_tab[data_idx].sh_addr = data_vaddr;
        if (bss_idx) shdr_tab[bss_idx].sh_addr = data_vaddr + data_bytes.size();
        if (text_idx) shdr_tab[text_idx].sh_offset = text_offset;
        if (rodata_idx) shdr_tab[rodata_idx].sh_offset = rodata_offset;
        if (data_idx) shdr_tab[data_idx].sh_offset = data_offset;
        if (bss_idx) shdr_tab[bss_idx].sh_offset = 0;
    } else {
        headers_size = sizeof(elf::Ehdr<E>);
        text_offset = headers_size;
        rodata_offset = text_offset + text_bytes.size();
        data_offset = rodata_offset + rodata_bytes.size();

        if (text_idx) shdr_tab[text_idx].sh_offset = text_offset;
        if (rodata_idx) shdr_tab[rodata_idx].sh_offset = rodata_offset;
        if (data_idx) shdr_tab[data_idx].sh_offset = data_offset;
        if (bss_idx) shdr_tab[bss_idx].sh_offset = 0;
    }

    size_t symtab_offset = align_addr(data_offset + data_bytes.size(), PAGE);
    size_t strtab_offset = align_addr(symtab_offset + symtab.size() * sizeof(elf::Sym<E>), PAGE);
    size_t shdr_offset = align_addr(strtab_offset + strtab.size(), PAGE);
    size_t shstrtab_offset = align_addr(shdr_offset + shdr_tab.size() * sizeof(elf::Shdr<E>), PAGE);

    shdr_tab[sym_idx].sh_offset = symtab_offset;
    shdr_tab[str_idx].sh_offset = strtab_offset;
    shdr_tab[str_idx].sh_size = strtab.size();
    shdr_tab[shstr_idx].sh_offset = shstrtab_offset;
    shdr_tab[shstr_idx].sh_size = shstrtab.size();

    size_t total_size = shstrtab_offset + shstrtab.size();

    elf::Ehdr<E> ehdr{};
    std::memcpy(ehdr.e_ident,
                "\x7f\x45\x4c\x46\x02\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);
    ehdr.e_type = elf_type;
    ehdr.e_machine = 62;
    ehdr.e_version = 1;
    ehdr.e_entry = is_rel ? 0 : ctx.symbol_map["_start"].addr;
    ehdr.e_phoff = is_rel ? 0 : sizeof(elf::Ehdr<E>);
    ehdr.e_phentsize = is_rel ? 0 : sizeof(elf::Phdr<E>);
    ehdr.e_phnum = is_rel ? 0 : static_cast<u16>(phdrs.size());
    ehdr.e_shoff = shdr_offset;
    ehdr.e_shentsize = sizeof(elf::Shdr<E>);
    ehdr.e_shnum = static_cast<u16>(shdr_tab.size());
    ehdr.e_shstrndx = shstr_idx;

    int fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0755);
    if (fd == -1) throw std::runtime_error("Failed to open output file");
    if (ftruncate(fd, total_size) != 0) {
        close(fd);
        throw std::runtime_error("Failed to resize file");
    }
    void* map = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("mmap failed");
    }
    close(fd);

    std::vector<std::function<void()>> copy_tasks;

    copy_tasks.push_back([&]() {
        std::memcpy(static_cast<char*>(map), &ehdr, sizeof(ehdr));
        if (!phdrs.empty())
            std::memcpy(static_cast<char*>(map) + sizeof(ehdr),
                        phdrs.data(), phdrs.size() * sizeof(elf::Phdr<E>));
    });

    if (!text_bytes.empty())
        copy_tasks.push_back([&]() {
            std::memcpy(static_cast<char*>(map) + text_offset,
                        text_bytes.data(), text_bytes.size());
        });
    if (!rodata_bytes.empty())
        copy_tasks.push_back([&]() {
            std::memcpy(static_cast<char*>(map) + rodata_offset,
                        rodata_bytes.data(), rodata_bytes.size());
        });
    if (!data_bytes.empty())
        copy_tasks.push_back([&]() {
            std::memcpy(static_cast<char*>(map) + data_offset,
                        data_bytes.data(), data_bytes.size());
        });

    copy_tasks.push_back([&]() {
        std::memcpy(static_cast<char*>(map) + symtab_offset,
                    symtab.data(), symtab.size() * sizeof(elf::Sym<E>));
    });
    copy_tasks.push_back([&]() {
        std::memcpy(static_cast<char*>(map) + strtab_offset,
                    strtab.data(), strtab.size());
    });
    copy_tasks.push_back([&]() {
        std::memcpy(static_cast<char*>(map) + shdr_offset,
                    shdr_tab.data(), shdr_tab.size() * sizeof(elf::Shdr<E>));
    });
    copy_tasks.push_back([&]() {
        std::memcpy(static_cast<char*>(map) + shstrtab_offset,
                    shstrtab.data(), shstrtab.size());
    });

    ctx.thread_pool.submit_all(copy_tasks);
    msync(map, total_size, MS_SYNC);
    munmap(map, total_size);
    chmod(path.c_str(), 0755);
}


template class OutputFile<arch::x86_64>;
template class OutputFile<arch::i386>;
} // namespace weld
