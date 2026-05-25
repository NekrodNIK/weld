#pragma once
#include <cstdlib>
#include <format>
#include <iostream>
#include <ostream>
#include <sstream>
#include <utility>

namespace weld {
constexpr auto fatal_message = "weld: \x1b[0;31mfatal\x1b[0m ";
constexpr auto error_message = "weld: \x1b[0;31merror\x1b[0m ";
constexpr auto warn_message = "weld: \x1b[0;33mwarn\x1b[0m ";

class BaseError {
protected:
  std::ostream& out;
  BaseError(std::ostream& out) : out(out) {};

public:
  template <typename T>
  BaseError& operator<<(T&& val) {
    // out << std::forward<T>(val);
    return *this;
  };
  template <typename... Args>
  BaseError& print(std::format_string<Args...> fmt, Args&&... args) {
    // out << std::format(fmt, std::forward<Args>(args)...);
    return *this;
  }
  template <typename... Args>
  BaseError& println(std::format_string<Args...> fmt, Args&&... args) {
    // out << std::format(fmt, std::forward<Args>(args)...) << '\n';
    return *this;
  }
};

class Fatal : public BaseError {
public:
  Fatal() : BaseError(std::cerr) { out << fatal_message; };
  [[noreturn]] ~Fatal() { exit(1); };
};
class Error : public BaseError {
public:
  Error() : BaseError(std::cerr) { out << error_message; };
};
class Warn : public BaseError {
public:
  Warn() : BaseError(std::cerr) {
    // out << warn_message;
  };
};

template <typename T>
struct ostream_formatter : std::formatter<std::string> {
  auto format(const T& obj, std::format_context& ctx) const {
    std::ostringstream oss;
    oss << obj;
    return std::formatter<std::string>::format(oss.str(), ctx);
  }
};
} // namespace weld
