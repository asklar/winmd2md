#include "pch.h"
#include <iostream>
#include <filesystem>
#include <memory>
#include <sstream>
#include <unordered_map>

#include "CppUnitTest.h"
#include "../winmd2markdown/Program.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

std::unordered_map<std::string, std::shared_ptr<std::ostringstream>> out_map{};

std::shared_ptr<std::ostream> GetOutputStream(const std::filesystem::path& name)
{
	out_map[name.u8string()] = make_shared<ostringstream>();
	return out_map[name.u8string()];
}

TEST_CLASS(UnitTests)
{
	static Program program;
public:

	TEST_CLASS_INITIALIZE(Setup)
	{
		program.Process({ "..\\..\\x64\\Debug\\Test\\Test.winmd" });
	}

	TEST_METHOD(SanityCheck)
	{
		assert(out_map.size() == 3);
		assert(out_map.find("out\\Class1-api-windows.md") != out_map.end());
		assert(out_map.find("out\\Interface1-api-windows.md") != out_map.end());
		assert(out_map.find("out\\Class2-api-windows.md") != out_map.end());
	}

	TEST_METHOD(TestClass1)
	{
		auto class1 = out_map["out\\Class1-api-windows.md"]->str();
		assert(class1 == R"(---
id: Class1
title: Class1
---

Kind: `class`



Class doc_string

## Properties
### MyProperty
 int `MyProperty`

**Default value**: `MyProperty value`

MyProperty doc_string


## Constructors
### Class1
 **`Class1`**()





)");
	}

	TEST_METHOD(TestInterface1) {
		auto interface1 = out_map["out\\Interface1-api-windows.md"]->str();
		assert(interface1 == R"(---
id: Interface1
title: Interface1
---

Kind: `interface`

Implemented by: 
- [`Class2`](Class2)


Interface1 doc_string



## Methods
### f
void **`f`**()

f() doc_string




)");
	}

	TEST_METHOD(TestClass2) {
		auto class2 = out_map["out\\Class2-api-windows.md"]->str();
		assert(class2 == R"(---
id: Class2
title: Class2
---

Kind: `class`

Implements: [`Interface1`](Interface1)

Class2 implements [`Interface1`](Interface1)



## Methods
### f
void **`f`**()

f() doc_string




)");

	}
};

Program UnitTests::program;
