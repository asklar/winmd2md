#include "output.h"
#include "Format.h"
#include "Options.h"

using namespace std;

output::output() {
  std::error_code ec;
  std::filesystem::create_directory("out", ec); // ignore ec
}

output::type_helper output::StartType(std::string_view name, std::string_view kind) {
  if (currentFile.is_open()) {
    EndType();
  }
  indents = 0;
  std::filesystem::path out("out");
  const string filename = std::string(name) + g_opts->fileSuffix + ".md";
  currentFile = std::ofstream(out / filename);
  currentFile << "---\n" <<
    "id: " << name << "\n" <<
    "title: " << name << "\n" <<
    "---\n\n";
  currentFile << "Kind: " << code(kind) << "\n\n";
  return type_helper(*this);
}

output::section_helper output::StartSection(const std::string& a) {
  return section_helper(*this, a);
}

output::section_helper::section_helper(output& out, string s) : o(out) {
  o.indents++;
  if (!s.empty()) {
    string t(o.indents, '#');
    o << t << " " << s << "\n";
  }
}

output::type_helper::type_helper(output& out) : o(out), sh(o.StartSection("")) {};