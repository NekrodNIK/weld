#include "elf.h"
#include "src/arch.h"

namespace weld::elf {
bool is_elf(std::span<u8> mem) {
  if (mem.size() < 4) {
    return false;
  }
  return (mem[EI_MAG0] == '\x7f') && (mem[EI_MAG1] == 'E') &&
         (mem[EI_MAG2] == 'L') && (mem[EI_MAG3] == 'F');
}

arch::Enum get_arch(std::span<u8> mem) {
  if (mem.size() < EI_IDENT + 2) {
    return arch::Enum::unsupported;
  }

  u16 e_machine = *reinterpret_cast<u16*>(mem.data() + EI_IDENT + 2);
  switch (e_machine) {
  case EM_386:
    return arch::Enum::i386;
  case EM_X86_64:
    return arch::Enum::x86_64;
  default:
    return arch::Enum::unsupported;
  }
}
} // namespace weld::elf
