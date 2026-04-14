#include "arch.h"

namespace weld::arch {
std::ostream& operator<<(std::ostream& out, Enum arch) {
  switch (arch) {
  case Enum::i386:
    out << i386::name;
    break;
  case Enum::x86_64:
    out << x86_64::name;
    break;
  case Enum::unsupported:
    out << "unsupported";
  }
  return out;
}
} // namespace weld::arch
