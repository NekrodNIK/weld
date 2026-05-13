#include "src/arch.h"
#include <filesystem>
#include <optional>
#include <vector>

struct LinkerFlags {
  // no @file option
  // std::filesystem::path file_with_options;
  bool export_dynamic;
  std::filesystem::path output_file;
  bool relocatable;
  // no scriptfile
  // std::filesystem::path scriptfile
  bool version;
  bool no_standart_library;
  // no pie
  // bool position_independent_executable;
  std::vector<std::filesystem::path> input_paths;
  std::optional<weld::arch::Enum> arch;
  std::string arch_name; // for debug purposes
  int num_threads = 0;

  LinkerFlags(std::vector<std::filesystem::path> sources,
              std::filesystem::path destination, bool export_dyn, bool relocate,
              bool nostdlib, bool version_, weld::arch::Enum arch_,
              std::string a_name)
      : input_paths(sources), output_file(destination), arch(arch_),
        export_dynamic(export_dyn), relocatable(relocate),
        no_standart_library(nostdlib), version(version_), arch_name(a_name),
        num_threads(1) {}

  LinkerFlags() {}

  void print();
};

LinkerFlags* parse_cli_options(int argc, char* argv[]);
