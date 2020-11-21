#pragma once
#include <vector>
#include <iostream>

struct options;
struct option
{
  std::string name;
  std::string description;
  int nArgs;
  void (*setter)(options*, std::string value);
};

const std::vector<option> get_option_names();

struct options
{
  bool outputExperimental{ false };
  bool propertiesAsTable{ false };
  bool fieldsAsTable{ false };
  bool help{ false };
  std::string sdkVersion;
  std::string winMDPath;
  std::string fileSuffix{ "-api-windows" };

  options(const std::vector<std::string>& v) {
    for (size_t i = 0; i < v.size(); i++) {
      const auto& o = v[i];
      if (o.empty()) continue;
      if (o[0] == '/' || o[0] == '-') {
        auto const opt = std::find_if(get_option_names().cbegin(), get_option_names().cend(), [&o](auto&& x) { return x.name == o.c_str() + 1; });
        if (opt != get_option_names().cend()) {
          if (opt->nArgs == 0) {
            opt->setter(this, {});
          }
          else {
            if (i < v.size() - 1) {
              opt->setter(this, v[++i]);
            }
            else {
              std::cerr << "Expected argument for option " << v[i] << "\n";
              std::abort();
            }
          }
        }
        else {
          std::cerr << "Unknown option: " << o << "\n";
          std::abort();
        }
      }
      else {
        if (winMDPath.empty()) {
          winMDPath = o;
        }
        else {
          std::cerr << "WinMD path already specified as " << winMDPath << " when we encountered " << o << "\n";
          std::abort();
        }
      }
    }
  }
};

extern options* g_opts;

