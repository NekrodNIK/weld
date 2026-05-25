#include "cli.h"
#include "elf.h"
#include "src/arch.h"
#include "src/errors.h"
#include "src/thread-pool.h"
#include "weld.h"
#include <cassert>
#include <memory>
#include <vector>

namespace weld {

template <typename E>
std::vector<std::unique_ptr<InputFile<E>>>
process_input(std::vector<MappedFile>& mapped_files, ThreadPool& pool) {
  std::vector<std::future<std::unique_ptr<InputFile<E>>>> futures;
  std::vector<std::unique_ptr<InputFile<E>>> inputs;
  std::mutex input_mutex;

  for (auto& mapped : mapped_files) {
    futures.push_back(pool.submit([&mapped, &input_mutex]() -> std::unique_ptr<InputFile<E>> {
      if (weld::elf::is_elf(mapped.data())) {
        auto input = weld::InputFile<E>::parse(std::move(mapped));
        if (!input) {
          std::lock_guard<std::mutex> lock(input_mutex);
          weld::Warn() << "Failed to parse file: " << mapped << '\n';
        }
        return input;
      } else {
        std::lock_guard<std::mutex> lock(input_mutex);
        weld::Warn() << "Skipping archive (not fully supported): " << mapped << '\n';
        return nullptr;
      }
    }));
  }

  for (auto& f : futures) {
    auto input = f.get();
    if (input) {
      inputs.push_back(std::move(input));
    }
  }

  return inputs;
}

template <typename E>
void main(std::vector<MappedFile>&& mapped_files, LinkerArgs& flags) {
  Context<E> ctx{
      .is_relocatable = flags.is_relocatable,
      .thread_pool = ThreadPool(flags.num_threads),
      .tasks = Tasks(ctx.thread_pool),
  };
  
  auto input_files = process_input<E>(mapped_files, ctx.thread_pool);
  auto output_file = weld::OutputFile<E>();

  for (auto& file : input_files) {
    file->resolve_symbols(ctx);
  }

  for (auto& file : input_files) {
    file->merge_sections(ctx);
  }

  output_file.resolve_relocations(ctx);
  output_file.write(ctx, flags.output_path);
}

} // namespace weld

std::vector<weld::MappedFile>
open_input(const std::vector<std::filesystem::path>& paths) {
  std::vector<weld::MappedFile> result;
  for (const auto& path : paths) {
    if (!std::filesystem::exists(path)) {
      weld::Fatal() << "file not found: " << path << '\n';
    }
    result.push_back(weld::MappedFile::open(path));
  }

  if (result.empty()) {
    weld::Fatal() << "no input files\n";
  }

  return result;
}

int main(int argc, char** argv) {
  auto flags = parse_cli_options(argc, argv);

  auto mapped_files = open_input(flags->input_paths);
  auto first_elf =
      std::find_if(mapped_files.begin(), mapped_files.end(), [](auto& mapped) {
        return weld::elf::is_elf(mapped.data());
      });

  if (first_elf == mapped_files.end()) {
    weld::Fatal() << "no ELF files found in input\n";
  }

  auto arch = flags->arch.value_or(weld::elf::get_arch_tag(first_elf->data()));

  switch (arch) {
  case weld::arch::Tag::i386:
    weld::main<weld::arch::i386>(std::move(mapped_files), *flags);
    break;
  case weld::arch::Tag::x86_64:
    weld::main<weld::arch::x86_64>(std::move(mapped_files), *flags);
    break;
  case weld::arch::Tag::unsupported:
    weld::Fatal() << *first_elf << " unsupported architecture\n";
  }

  return 0;
}
