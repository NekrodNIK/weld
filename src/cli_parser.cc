#include "cli_parser.h"
#include "cxxopts.h"
#include "src/arch.h"
#include "src/weld.h"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

class InvalidArchitecture : std::runtime_error {
  std::string message;

public:
  InvalidArchitecture(std::string arch_) : std::runtime_error("") {
    message = "Invalid architecture encountered: " + arch_;
  }

  const char* what() const noexcept override { return message.c_str(); }
};

void LinkerFlags::print() {
  std::cout << "export_dynamic = " << std::boolalpha << export_dynamic
            << std::endl;
  std::cout << "relocatable = " << std::boolalpha << relocatable << std::endl;
  std::cout << "version = " << std::boolalpha << version << std::endl;
  std::cout << "nostdlib = " << std::boolalpha << no_standart_library
            << std::endl;
  std::cout << "architecture: " << arch_name << std::endl;
  std::cout << output_file << std::endl;
  for (auto i : input_paths) {
    std::cout << "    " << i << std::endl;
  }
}

LinkerFlags* parse_cli_options(int argc, char* argv[]) {
  cxxopts::Options options("CLI-Parser", "Desc");

  // clang-format off
  options.add_options()
      ("E,export-dynamic", "Export dynamic", cxxopts::value<bool>()->default_value("false"))
      ("r,relocatable", "Relocatable result", cxxopts::value<bool>()->default_value("false"))
      ("nolib,nostdlib", "No standart library", cxxopts::value<bool>()->default_value("false"))
      ("v,version", "Version", cxxopts::value<bool>()->default_value("false"))
      ("o", "Output file", cxxopts::value<std::string>()->default_value("a.out"))
      ("input", "Input files", cxxopts::value<std::vector<std::string>>())
      ("m,target", "Architecture", cxxopts::value<std::string>())
      ("j,threads", "Number of threads", cxxopts::value<int>()->default_value("0"))
  ;
  // clang-format on

  options.parse_positional({"input"});

  auto result = options.parse(argc, argv);

  LinkerFlags* lf = new LinkerFlags;

  lf->export_dynamic = result["export-dynamic"].as<bool>();
  lf->relocatable = result["relocatable"].as<bool>();
  lf->no_standart_library = result["nostdlib"].as<bool>();
  lf->version = result["version"].as<bool>();

  if (auto arch_opt = result["target"].as_optional<std::string>()) {
    auto arch_name = arch_opt.value();
    lf->arch_name = arch_name;
      
    if (arch_name == "elf_x86_64") {
      lf->arch = weld::arch::Enum::x86_64;
    } else if (arch_name == "elf_i386"){
      lf->arch = weld::arch::Enum::i386;
    } else {
      weld::Fatal() << "Unknown architecture";
    }
  } else {
    lf->arch = std::nullopt;
  }

  lf->num_threads = result["threads"].as<int>();
  if (lf->num_threads == 0) lf->num_threads = 1;
  lf->output_file = std::filesystem::path(result["o"].as<std::string>());

  std::vector<std::filesystem::path> sources;

  if (auto files_opt = result["input"].as_optional<std::vector<std::string>>()) {
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
