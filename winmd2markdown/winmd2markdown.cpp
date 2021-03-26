// winmd2markdown.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <filesystem>
#include <iostream>

#include "output.h"
#include "Options.h"
#include "Format.h"
#include "Program.h"
using namespace std;
using namespace winmd::reader;



std::shared_ptr<std::ostream> GetOutputStream(const std::filesystem::path& name)
{
  auto os = make_shared<ofstream>(name);
  if (!os->good()) {
    throw exception(("Failed to create file " + name.string()).c_str());
  }
  return os;
}

int main(int argc, char** argv)
{
  try {
    Program program;
    program.Process(std::vector<string>(argv + 1, argv + argc));

    return 0;
  }
  catch (const exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
