#include "src/arch.h"
#include "src/errors.h"
#include "weld.h"
#include <ar.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <ios>
#include <iostream>
#include <span>
#include <string>
#include <unistd.h>
#include <vector>

namespace {
#pragma pack(push, 1)
struct FileHeader {
  char ar_name[16];
  char ar_date[12];
  char ar_uid[6], ar_gid[6];
  char ar_mode[8];
  char ar_size[10];
  char ar_fmag[2];

  size_t size() const { return std::strtol(ar_size, nullptr, 10); }
};
#pragma pack(pop)
static_assert(sizeof(FileHeader) == 60);
} // namespace

namespace weld {
template <typename E>
bool ArchiveFile<E>::is_archive(std::span<u8> mem) {
  return mem.size() >= SARMAG && std::memcmp(mem.data(), ARMAG, SARMAG) == 0;
}

template <typename E>
ArchiveFile<E>::ArchiveFile(MappedFile&& mapped)
    : InputFile<E>(std::move(mapped)) {
  if (!is_archive(this->mapped_.data()))
    Fatal().println("[{}] file is not archive", mapped);

  auto slice = this->mapped_.data().subspan(SARMAG);
  FileHeader header;

  for (size_t index = 0; !slice.empty(); index++) {
    if (slice.size() < sizeof(FileHeader))
      Fatal().println("[{}] invalid archive member#{}", this->mapped_, index);
    std::memcpy(&header, slice.data(), sizeof(FileHeader));

    std::string name(header.ar_name, sizeof(header.ar_name));
    name.erase(name.find_last_not_of(' ') + 1);

    if (header.ar_fmag[0] != 0x60 || header.ar_fmag[1] != 0x0A)
      Fatal().println("[{}] invalid archive member#{}: {}", this->mapped_,
                      index, header.ar_name);

    size_t member_size = header.size();
    size_t total_size = sizeof(FileHeader) + member_size + (member_size % 2);

    if (slice.size() < total_size) {
      Warn().println("[{}] truncated member#{}: {}", this->mapped_, index,
                     header.ar_name);
      break;
    }
    slice = slice.subspan(sizeof(FileHeader));

    if (name != "/" && name != "//" && !name.empty())
      members.push_back(slice.subspan(0, member_size));
    slice = slice.subspan(member_size);

    if (member_size % 2 == 1)
      slice = slice.subspan(1);
  }

  if (!slice.empty()) {
    Warn().println("[{}] invalid archive size (header size != real size)",
                   mapped);
  }
}

template <typename E>
void ArchiveFile<E>::resolve_symbols(Context<E>& ctx) {
  for (auto member : members) {
  }
}

template <typename E>
void ArchiveFile<E>::merge_sections(Context<E>& ctx) {}
template class ArchiveFile<weld::arch::i386>;
template class ArchiveFile<weld::arch::x86_64>;
}; // namespace weld

class ArWriter {
public:
  static void write(const std::string& archname,
                    const std::vector<std::string>& filenames) {
    std::ofstream writer(archname, std::ios::binary);
    if (!writer) {
      throw std::ios::failure("Couldn't open archive file for writing: " +
                              archname);
    }
    writer.write(ARMAG, SARMAG);

    for (const auto& path : filenames) {
      std::ifstream fileReader(path, std::ios::binary);
      if (!fileReader) {
        throw std::ios::failure("Couldn't open file for reading:" + path);
      }

      fileReader.seekg(0, std::ios::end);
      size_t size = fileReader.tellg();
      fileReader.seekg(0, std::ios::beg);
      std::vector<char> data(size);
      fileReader.read(data.data(), size);

      if (fileReader.gcount() != size) {
        throw std::ios::failure("Couldn't read entire file: " + path);
      }

      std::string name = path;
      int pos = name.find_last_of("/\\");
      if (pos != std::string::npos) {
        name = name.substr(pos + 1);
      }

      if (name.size() > 16) {
        throw std::invalid_argument("Filename: " + name + " is too long");
      }

      FileHeader fileHeader;
      std::memset(fileHeader.ar_name, ' ', sizeof(fileHeader.ar_name));
      std::memcpy(fileHeader.ar_name, name.c_str(), name.size());
      std::memcpy(fileHeader.ar_fmag, "`\n", 2);

      char tmp[16];
      int len;

      len = snprintf(tmp, sizeof(tmp), "%12ld", std::time(nullptr));
      memcpy(fileHeader.ar_date, tmp,
             std::min<size_t>(len, sizeof(fileHeader.ar_date)));

      len = snprintf(tmp, sizeof(tmp), "%6d", getuid());
      memcpy(fileHeader.ar_uid, tmp,
             std::min<size_t>(len, sizeof(fileHeader.ar_uid)));

      len = snprintf(tmp, sizeof(tmp), "%6d", getgid());
      memcpy(fileHeader.ar_gid, tmp,
             std::min<size_t>(len, sizeof(fileHeader.ar_gid)));

      len = snprintf(tmp, sizeof(tmp), "%8o", 0644);
      memcpy(fileHeader.ar_mode, tmp,
             std::min<size_t>(len, sizeof(fileHeader.ar_mode)));

      len = snprintf(tmp, sizeof(tmp), "%10zu", data.size());
      memcpy(fileHeader.ar_size, tmp,
             std::min<size_t>(len, sizeof(fileHeader.ar_size)));

      writer.write(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
      writer.write(data.data(), data.size());

      if (data.size() % 2 != 0) {
        writer.put('\n');
      }
    }
  }
};
