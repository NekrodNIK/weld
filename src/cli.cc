#include "cli.h"
#include "cxxopts.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <vector>

void LinkerArgs::print() {
  std::print("input_paths = [");
  for (auto path : input_paths) {
    std::print("{}, ", path.string());
  }
  std::println("]");
  std::println("output_path = {}", output_path.string());
  std::println("arch = {}", [this] {
    return arch ? std::format("{}", arch.value()) : "None";
  }());
  std::println("relocatable = {}", relocatable);
  std::println("export_dynamic = {}", export_dynamic);
  std::println("pie = {}", pie);
  std::println("whole_archive = {}", whole_archive);
}

std::unique_ptr<LinkerArgs> parse_cli_options(int argc, char* argv[]) {
  cxxopts::Options options("weld", "Well, linker!");

  // clang-format off
  options.add_options()
      ("v,version", "Version", cxxopts::value<bool>()->default_value("false"))
      ("h,help", "Print help message", cxxopts::value<bool>()->default_value("false"))
      ("E,export-dynamic", "Export dynamic", cxxopts::value<bool>()->default_value("false"))
      ("r,relocatable", "Relocatable result", cxxopts::value<bool>()->default_value("false"))
      ("o", "Output file", cxxopts::value<std::string>()->default_value("a.out"))
      ("input", "Input files", cxxopts::value<std::vector<std::string>>())
      ("m,target", "Architecture", cxxopts::value<std::string>())
      ("pie", "Position Independent Executable", cxxopts::value<bool>()->default_value("false"))
      ("whole-archive", "Using whole archive", cxxopts::value<bool>()->default_value("false"))
  ;
  // clang-format on
  options.parse_positional({"input"});
  auto result = options.parse(argc, argv);

  if (result["help"].as<bool>()) {
    std::print("{}", options.help());
    exit(0);
  }
  
  if (result["version"].as<bool>()) {
    std::println("{}", "0.0.1");
    exit(0);
  }

  auto lf = std::make_unique<LinkerArgs>();

  lf->export_dynamic = result["export-dynamic"].as<bool>();
  lf->relocatable = result["relocatable"].as<bool>();

  if (auto arch_opt = result["target"].as_optional<std::string>()) {
    auto arch_name = arch_opt.value();

    if (arch_name == "elf_x86_64") {
      lf->arch = weld::arch::Enum::x86_64;
    } else if (arch_name == "elf_i386") {
      lf->arch = weld::arch::Enum::i386;
    } else {
      weld::Fatal() << "Unknown architecture";
    }
  } else {
    lf->arch = std::nullopt;
  }

  lf->output_path = std::filesystem::path(result["o"].as<std::string>());

  std::vector<std::filesystem::path> sources;

  if (auto files_opt =
          result["input"].as_optional<std::vector<std::string>>()) {
    auto files = files_opt.value();
    for (const auto file : files) {
      sources.push_back(std::filesystem::path(file));
    }
  } else {
    weld::Fatal() << "No input files";
  }

  lf->input_paths = sources;

  return lf;
}
