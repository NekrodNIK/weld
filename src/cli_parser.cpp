#include "cli_parser.hpp"
#include <iostream>

//#define print(str) std::cout << str << std::endl

void LinkerFlags::print() {
    std::cout << "export_dynamic = " << std::boolalpha << export_dynamic << std::endl;
    std::cout << "relocatable = " << std::boolalpha << relocatable << std::endl;
    std::cout << "version = " << std::boolalpha << version << std::endl;
    std::cout << "nostdlib = " << std::boolalpha << no_standart_library << std::endl;
    std::cout << output_file << std::endl;
    for (auto i : object_files) {
        std::cout << "    " << i << std::endl;
    }
}

LinkerFlags* parse_cli_options(int argc, char* argv[]) {
    cxxopts::Options options("CLI-Parser", "Desc");

    options.add_options()
        ("E,export-dynamic", "Export dynamic", cxxopts::value<bool>()->default_value("false"))
        ("r,relocatable", "Relocatable result", cxxopts::value<bool>()->default_value("false"))
        ("nolib,nostdlib", "No standart library", cxxopts::value<bool>()->default_value("false"))
        ("v,version", "Version", cxxopts::value<bool>()->default_value("false"))
        ("o", "Output file", cxxopts::value<std::string>()->default_value("./a.out"))
        ("input", "Input files", cxxopts::value<std::vector<std::string>>())
    ;

    options.parse_positional({"input"});

    auto result = options.parse(argc, argv);

    LinkerFlags* lf = new LinkerFlags;

    lf->export_dynamic = result["export-dynamic"].as<bool>();
    lf->relocatable = result["relocatable"].as<bool>();
    lf->no_standart_library = result["nostdlib"].as<bool>();
    lf->version = result["version"].as<bool>();

    lf->output_file = result.contains("o") ? std::filesystem::path(result["o"].as<std::string>()) : std::filesystem::path();

    std::vector<std::filesystem::path> sources;

    const auto files = result["input"].as<std::vector<std::string>>();
    for (const auto file : files) {
        sources.push_back(std::filesystem::path(file));
    }

    lf->object_files = sources;

    return lf;
}
