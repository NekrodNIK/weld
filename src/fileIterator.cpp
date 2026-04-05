#include "weld.h"
#include "statLibParser.hpp"
#include "cli_parser.hpp"
#include <vector>
#include <memory>


std::vector<std::unique_ptr<weld::InputFile>> processInputFiles(const std::vector<std::filesystem::path>& paths) {
    std::vector<std::unique_ptr<weld::InputFile>> inputs;
    std::vector<weld::MappedFile> archives;

    for (const auto& path : paths) {
        auto mappedOpt = weld::MappedFile::open(path);
        if (!mappedOpt) {
            weld::Warn() << "Cannot opern file: " << path << std::endl;
            continue;
        }
        weld::MappedFile file = std::move(*mappedOpt);

        if (file.size() >= SARMAG && std::memcmp(file.raw(), ARMAG, SARMAG) == 0) {
            auto members = ArReader::extractMembers(file);
            archives.push_back(std::move(file));
            for (auto& [name, memberFile] : members) {
                auto input = weld::InputFile::parse(std::move(memberFile));
                if (input) {
                    inputs.push_back(std::move(input));
                } else {
                    weld::Warn() << "Failed to parse archive member: " << name << " from " << path << std::endl;
                }
            }
        }
        else if (file.size() >= 4 &&
                 file.raw()[0] == 0x7F && file.raw()[1] == 'E' &&
                 file.raw()[2] == 'L' && file.raw()[3] == 'F') {
            auto input = weld::InputFile::parse(std::move(file));
            if (input) {
                inputs.push_back(std::move(input));
            } else {
                weld::Warn() << "Failed to parse ELF file: " << path << std::endl;
            }
        }
        else {
            weld::Warn() << "Unknown file type: " << path << std::endl;
        }
    }
    return inputs;
    
}