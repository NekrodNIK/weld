#include "doctest.h"
#include "src/arch.h"
#include "src/weld.h"
#include <tuple>

namespace weld {

template<typename E>
void InputSection<E>::scan_relocations(Context<E>& ctx) {
  
}

template <typename E>
void InputSection<E>::write_to(Context<E>& ctx, std::span<u8> buf) {
}
} // namespace weld

