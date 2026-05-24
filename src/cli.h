#pragma once
#include "arch.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

struct LinkerArgs {
  std::vector<std::filesystem::path> input_paths;
  std::filesystem::path output_path;
  std::optional<weld::arch::Tag> arch;

  bool relocatable;
  bool export_dynamic;
  bool pie;
  bool whole_archive;
  int num_threads;

  void print();
};

std::unique_ptr<LinkerArgs> parse_cli_options(int argc, char* argv[]);
