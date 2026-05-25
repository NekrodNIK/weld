#include "arch.h"

namespace weld::arch {
std::ostream& operator<<(std::ostream& out, Tag arch) {
  switch (arch) {
  case Tag::i386:
    out << i386::name;
    break;
  case Tag::x86_64:
    out << x86_64::name;
    break;
  case Tag::unsupported:
    out << "unsupported";
  }
  return out;
}
} // namespace weld::arch
