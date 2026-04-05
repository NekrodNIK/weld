#include "src/arch.h"
#include "src/elf.h"
#include "src/weld.h"
#include <array>

template<typename E>
int weld_main() {
  using namespace weld;
  std::array<MappedFile, 2> mems{MappedFile::open("1.o").value(),
                                 MappedFile::open("2.o").value()};
  for (auto&& mem : mems) {
    InputFile<E>::parse(std::move(mem));
  }
}

int main() {
  using namespace weld;

  // FIXME: detect arch
  auto arch = arch::Enum::i386;
  switch (arch) {
  case arch::Enum::i386:
    return weld_main<arch::i386>();
  case arch::Enum::x86_64:
    return weld_main<arch::x86_64>();
  default:
    Fatal() << "unsupported architecture";
  };
  return 0;
}
