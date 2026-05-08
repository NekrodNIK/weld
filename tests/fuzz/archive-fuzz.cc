#include "src/arch.h"
#include "src/errors.h"
#include "src/weld.h"
#include <cstddef>
#include <cstdint>
#include <span>

// weld::Fatal::~Fatal() {
  
// }

extern "C" int LLVMFuzzerTestOneInput(uint8_t* Data, size_t Size) {
  auto mapped = weld::MappedFile::from_span(std::span<uint8_t>(Data, Size));
  auto ar = weld::ArchiveFile<weld::arch::x86_64>(std::move(mapped));
  (void)ar;
  return 0;
}
