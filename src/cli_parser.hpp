#include "cxxopts.hpp"
#include <filesystem>
#include <vector>

enum Arch {
    elf_x86_64
};

struct LinkerFlags {
    // no @file option
    //std::filesystem::path file_with_options;
    bool export_dynamic;
    std::filesystem::path output_file;
    bool relocatable;
    // no scriptfile
    // std::filesystem::path scriptfile
    bool version;
    bool no_standart_library;
    // no pie
    // bool position_independent_executable;
    std::vector<std::filesystem::path> object_files;
    Arch arch;
    std::string arch_name; // for debug purposes 

    LinkerFlags(std::vector<std::filesystem::path> sources, std::filesystem::path destination,
                bool export_dyn, bool relocate, bool nostdlib, bool version_, Arch arch_, std::string a_name) : object_files(sources),
                output_file(destination), arch(arch_) , export_dynamic(export_dyn), relocatable(relocate),
                no_standart_library(nostdlib), version(version_), arch_name(a_name) {}

    LinkerFlags() {}

    void print();
};


LinkerFlags* parse_cli_options(int argc, char* argv[]);