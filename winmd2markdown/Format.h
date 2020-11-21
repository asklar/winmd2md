#pragma once
#include <string_view>
#include <string>

inline std::string code(std::string_view v) {
  return "`" + std::string(v) + "`";
}
