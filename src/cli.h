#pragma once
#include "arch.h"
#include <filesystem>
#include <optional>
#include <vector>

struct LinkerArgs {
  std::vector<std::filesystem::path> input_paths;
  std::filesystem::path output_path;
  std::optional<weld::arch::Enum> arch;

  bool relocatable;
  bool export_dynamic;
  bool pie;
  bool whole_archive;

  void print();
};

LinkerArgs* parse_cli_options(int argc, char* argv[]);
