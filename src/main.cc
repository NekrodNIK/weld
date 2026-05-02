#include "cli.h"
#include "elf.h"
#include "src/arch.h"
#include "weld.h"
#include <cassert>
#include <memory>
#include <vector>

namespace weld {

template <typename E>
std::vector<std::unique_ptr<InputFile<E>>>
process_input(std::vector<MappedFile>& mapped_files) {
  std::vector<std::unique_ptr<weld::InputFile<E>>> inputs;

  for (auto& mapped : mapped_files) {
    // if (::isArFile(std::string{mapped.filename()})) {
    //   auto members = ArReader::extractMembers(mapped);
    //   Warn() << members.size() << '\n';
    //   for (auto& [name, memberFile] : members) {
    //     auto input = weld::InputFile<E>::parse(std::move(memberFile));
    //     if (input) {
    //       inputs.push_back(std::move(input));
    //     } else {
    //       weld::Warn() << "Failed to parse archive member: " << name << "
    //       from "
    //                    << mapped.filename() << '\n';
    //     }
    //   }
    // } else
    if (weld::elf::is_elf(mapped.data())) {
      auto input = weld::InputFile<E>::parse(std::move(mapped));
      if (input) {
        inputs.push_back(std::move(input));
      } else {
        weld::Warn() << "Failed to parse ELF file: " << mapped << '\n';
      }
    } else {
      weld::Warn() << "Unknown file type: " << mapped << '\n';
    }
  }
  return inputs;
}

template <typename E>
void main(std::vector<MappedFile>&& mapped_files, LinkerArgs& flags) {
  auto input_files = process_input<E>(mapped_files);
  auto output_file = weld::OutputFile<E>();
  Context<E> ctx;

  for (auto& file : input_files) {
    file->resolve_symbols(ctx);
    file->merge_sections(ctx);
  }

  output_file.resolve_relocations(ctx);
  output_file.write(ctx, flags.output_path);
}

} // namespace weld

std::vector<weld::MappedFile>
open_input(const std::vector<std::filesystem::path>& paths) {
  std::vector<weld::MappedFile> result;
  for (const auto& path : paths)
    result.push_back(weld::MappedFile::open(path));
  return result;
}

int main(int argc, char** argv) {
  auto flags = parse_cli_options(argc, argv);

  auto mapped_files = open_input(flags->input_paths);
  // FIXME: Add detection the archive architecture (and generally ensure closer
  // integration of archive processing with other parts of the project).
  auto first_elf =
      std::find_if(mapped_files.begin(), mapped_files.end(), [](auto& mapped) {
        return weld::elf::is_elf(mapped.data());
      });
  auto arch = flags->arch.value_or(weld::elf::get_arch(first_elf->data()));
  assert(first_elf != mapped_files.end());

  switch (arch) {
  case weld::arch::Enum::i386:
    weld::main<weld::arch::i386>(std::move(mapped_files), *flags);
    break;
  case weld::arch::Enum::x86_64:
    weld::main<weld::arch::x86_64>(std::move(mapped_files), *flags);
    break;
  case weld::arch::Enum::unsupported:
    weld::Fatal() << *first_elf << " unsupported architecture";
  }
}
