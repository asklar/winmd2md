#pragma once
#include <filesystem>
#include <string>
#include <fstream>

struct output
{
private:
  struct type_helper;
  struct section_helper;
public:
  output();
  std::ofstream currentFile;
  type_helper StartType(std::string_view name, std::string_view kind);

  section_helper StartSection(const std::string& a);

  template<typename T>
  friend output& operator<<(output& o, const T& t);

private:
  int indents = 0;
  void EndSection() {
    indents--;
  }
  void EndType() {
    currentFile.flush();
    currentFile.close();
  }
  friend struct type_helper;
  struct section_helper {
    output& o;
    section_helper(output& out, std::string s);
    ~section_helper() {
      o.indents--;
    }
  };
  struct type_helper {
    output& o;
    section_helper sh;
    type_helper(output& out);
    ~type_helper() {
      o.EndType();
    }
  };
};
