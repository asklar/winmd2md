#pragma once
#include <filesystem>
#include <string>
#include <fstream>

enum class MemberType
{
  Property,
  Type,
  Field,
  Method
};

struct intellisense_xml
{
  std::ofstream out;
  intellisense_xml() = default;
  intellisense_xml(intellisense_xml&&) = default;
  intellisense_xml& operator=(intellisense_xml&&) noexcept = default;
  intellisense_xml(std::string_view _namespaceName)  : namespaceName(_namespaceName)
  {
    out = std::ofstream("out\\" + namespaceName + ".xml");
    out << R"(<?xml version="1.0" encoding="utf-8"?>
<doc>
  <assembly>
    <name>)" << namespaceName << R"(</name>
  </assembly>
  <members>)";
  }

  void AddMember(MemberType mt, std::string shortName, std::string data);

  ~intellisense_xml() {
    out << R"(
  </members>
</doc>)" << std::endl;
  }

private:
  std::string namespaceName;

  std::string Sanitize(std::string text);
  char ToString(MemberType mt)
  {
    switch (mt)
    {
    case MemberType::Field:
      return 'F';
    case MemberType::Property:
      return 'P';
    case MemberType::Type:
      return 'T';
    case MemberType::Method:
      return 'M';
    default:
      throw std::invalid_argument("unexpected member type");
    }
  }
};

struct output
{
private:
  struct type_helper;
  struct section_helper;
public:
  output();
  std::ofstream currentFile;
  intellisense_xml currentXml;
  type_helper StartType(std::string_view name, std::string_view kind);

  section_helper StartSection(const std::string& a);

  template<typename T>
  friend output& operator<<(output& o, const T& t);

  void StartNamespace(std::string_view namespaceName);

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
