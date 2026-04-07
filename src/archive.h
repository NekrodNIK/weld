#include "weld.h"
#include <ar.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

bool isArFile(const std::string& filename) {
  std::ifstream reader(filename, std::ios::binary);
  if (!reader) {
    return false;
  }
  char head[SARMAG];
  reader.read(head, SARMAG);
  return reader.gcount() == SARMAG && std::memcmp(head, ARMAG, SARMAG) == 0;
}

#pragma pack(push, 1)
struct FileHeader {
  char ar_name[16];
  char ar_date[12];
  char ar_uid[6], ar_gid[6];
  char ar_mode[8];
  char ar_size[10];
  char ar_fmag[2];
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 60);

struct ArchiveMember {
  std::string name;
  std::vector<char> data;

  void print() {
    std::cout << name << std::endl;
    std::cout << "data = ... (size = " << data.size() << " )" << std::endl;
  }

  bool operator==(ArchiveMember& other) {
    if (name != other.name || data.size() != other.data.size()) {
      return false;
    }
    for (int i = 0; i < data.size(); i++) {
      if (data[i] != other.data[i]) {
        return false;
      }
    }
    return true;
  }
};

struct Archive {
  std::vector<ArchiveMember> files;

  bool operator==(Archive& other) { return true; }

  void print() {
    for (auto a : files) {
      a.print();
    }
  }
};

class ArReader {
public:
  static Archive read(const std::string& filename) {
    if (!isArFile(filename)) {
      throw std::ios_base::failure("Couldn't open file: " + filename);
    }
    std::vector<ArchiveMember> members;
    std::ifstream reader(filename, std::ios::binary);
    reader.seekg(8, std::ios::beg);
    while (reader.peek() != EOF) {
      FileHeader header;
      reader.read(reinterpret_cast<char*>(&header), sizeof(header));
      if (reader.gcount() != sizeof(header)) {
        break;
      }
      if (header.ar_fmag[0] != 0x60 || header.ar_fmag[1] != 0x0A) {
        throw std::ios::failure("Invalid archive. Failed on index " +
                                std::to_string(reader.peek()));
      }

      char sizeChar[11] = {0};
      std::memcpy(sizeChar, header.ar_size, 10);
      long size = std::strtol(sizeChar, nullptr, sizeof(FileHeader::ar_size));

      char nameChar[17] = {0};
      std::memcpy(nameChar, header.ar_name, sizeof(FileHeader::ar_name));
      std::string name(nameChar);
      name.erase(name.find_last_not_of(' ') + 1);

      if (name == "/" || name == "//") {
        reader.seekg(size, std::ios::cur);
        if (size % 2 != 0) {
          reader.seekg(1, std::ios::cur);
        }
        continue;
      }

      std::vector<char> data(size);
      reader.read(data.data(), size);
      if (reader.gcount() != size) {
        throw std::ios::failure(
            "Invalid archive. File has diiferent size than in header");
      }

      if (size % 2 != 0) {
        char padding;
        reader.read(&padding, 1);
      }

      ArchiveMember member = {name, data};
      members.push_back(member);
    }
    return Archive{members};
  }

  static std::vector<std::pair<std::string, weld::MappedFile>>
  extractMembers(const weld::MappedFile& archive) {
    std::vector<std::pair<std::string, weld::MappedFile>> members;
    const weld::u8* data = archive.raw();
    size_t archiveSize = archive.size();

    if (archiveSize < SARMAG || std::memcmp(data, ARMAG, SARMAG) != 0) {
      throw std::runtime_error("Invalid MappedFile");
    }

    size_t offset = SARMAG;
    while (offset + sizeof(FileHeader) <= archiveSize) {
      FileHeader header;
      memcpy(&header, data + offset, sizeof(FileHeader));

      if (header.ar_fmag[0] != '`' || header.ar_fmag[1] != '\n') {
        break;
      }

      char sizeChar[11] = {0};
      memcpy(sizeChar, header.ar_size, 10);
      long size = strtol(sizeChar, nullptr, 10);

      char nameChar[17] = {0};
      memcpy(nameChar, header.ar_name, sizeof(FileHeader::ar_name));
      std::string name(nameChar);
      name.erase(name.find_last_not_of(' ') + 1);

      if (name == "/" || name == "//" || (name[0] == '/' && name.size() > 1)) {
        offset += sizeof(FileHeader) + size;
        if (size % 2 != 0)
          offset++;
        continue;
      }

      if (offset + sizeof(FileHeader) + size <= archiveSize) {
        char tmpname[] = "/tmp/weld_XXXXXX";
        int fd = mkstemp(tmpname);
        if (fd != -1) {
          write(fd, data + offset + sizeof(FileHeader), size);
          close(fd);

          weld::MappedFile slice = weld::MappedFile::open(tmpname);
          members.emplace_back(std::move(name), std::move(slice));

          unlink(tmpname);
        }
      }

      offset += sizeof(FileHeader) + size;
      if (size % 2 != 0)
        offset++;
    }
    return members;
  }
};

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
