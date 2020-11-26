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
  return make_shared<ofstream>(name);
}

int main(int argc, char** argv)
{
  g_program.Process(std::vector<string>(argv + 1, argv + argc));

  return 0;
}
