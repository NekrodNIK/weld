#include "weld.h"
#include <cstdlib>
#include <iostream>
#include <unistd.h>

constexpr auto fatal_message = "weld: \x1b[0;31mfatal\x1b[0m ";
constexpr auto error_message = "weld: \x1b[0;31merror\x1b[0m ";
constexpr auto warning_message = "weld: \x1b[0;31mwarning\x1b[0m ";

namespace weld {
Fatal::Fatal() : out(std::cerr) { out << fatal_message; }
Fatal::~Fatal() { exit(1); }
Error::Error() : out(std::cerr) { out << error_message; }
Warn::Warn() : out(std::cerr) { out << warning_message; };
} // namespace weld
