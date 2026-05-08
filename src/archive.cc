#include "src/arch.h"
#include "src/errors.h"
#include "src/ints.h"
#include "weld.h"
#include <ar.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <span>
#include <unistd.h>
#include <vector>

namespace {
struct [[gnu::packed]] Hdr {
  char ar_name[16];
  char ar_date[12];
  char ar_uid[6], ar_gid[6];
  char ar_mode[8];
  char ar_size[10];
  char ar_fmag[2];
};
static_assert(sizeof(Hdr) == 60);
} // namespace

namespace weld {
std::optional<ArchiveMember> ArchiveMember::parse(std::span<u8> mem) {
  if (mem.size() < sizeof(Hdr))
    return {};

  auto& hdr = *reinterpret_cast<Hdr*>(mem.data());
  if (hdr.ar_fmag[0] != 0x60 || hdr.ar_fmag[1] != 0x0A)
    return {};

  size_t size;
  auto result =
      std::from_chars(hdr.ar_size, hdr.ar_size + sizeof(hdr.ar_size), size);
  if (result.ec != std::errc{} || mem.size() < sizeof(Hdr) + size + size % 2)
    return {};

  auto name = std::string_view(hdr.ar_name, sizeof(hdr.ar_name));
  auto slash = name.find('/');
  if (slash != std::string_view::npos)
    name = name.substr(0, slash);
  name = name.substr(0, name.find_last_not_of(' ') + 1);

  ArchiveMember obj;
  obj.name_ = name;
  obj.mem_ = mem.subspan(sizeof(Hdr), size);
  return obj;
}

std::string_view ArchiveMember::name() const { return name_; }
std::span<u8> ArchiveMember::mem() const { return mem_; }
size_t ArchiveMember::total_size() const {
  return sizeof(Hdr) + mem_.size() + mem_.size() % 2;
}

template <typename E>
bool ArchiveFile<E>::is_archive(std::span<u8> mem) {
  return mem.size() >= SARMAG && std::memcmp(mem.data(), ARMAG, SARMAG) == 0;
}

template <typename E>
ArchiveFile<E>::ArchiveFile(MappedFile&& mapped)
    : InputFile<E>(std::move(mapped)) {
  if (!is_archive(this->mapped_.data()))
    Fatal().println("[{}] file is not archive", this->mapped_);

  auto slice = this->mapped_.slice(SARMAG, this->mapped_.size() - SARMAG);
  for (size_t index = 0; slice.size() > 0; index++) {
    ArchiveMember member;
    {
      auto result = ArchiveMember::parse(slice.data());
      if (!result)
        Fatal().println("[{}] invalid archive member#{}", this->mapped_, index);
      member = *result;
    }

    if (member.name() != "/" && member.name() != "//")
      members.push_back(member);
    slice =
        slice.slice(member.total_size(), slice.size() - member.total_size());
  }

  if (slice.size() > 0) {
    Warn().println("[{}] invalid archive size (header size != real size)",
                   this->mapped_);
  }
}

template <typename E>
void ArchiveFile<E>::resolve_symbols(Context<E>& ctx) {
  for (auto& [name, symbol] : ctx.symbol_map) {
    if (symbol.is_defined())
      continue;
    for (auto& member : members) {
      // auto file = ObjectFile<E>(MappedFile::ha member.mem());
      // if (file.has_non_local(name)) {
      //   file.resolve_symbols(ctx);
      //   file.merge_sections(ctx);
      // }
    }
  }
}

template <typename E>
void ArchiveFile<E>::merge_sections(Context<E>& ctx) {}
template class ArchiveFile<weld::arch::i386>;
template class ArchiveFile<weld::arch::x86_64>;
}; // namespace weld
