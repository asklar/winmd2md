#pragma once
#include <string_view>
#include <string>
#include <winmd_reader.h>

struct Program;

struct Formatter
{
  Formatter(Program* p) : program(p) {};

  std::string MakeMarkdownReference(const std::string& ns, const std::string& type, const std::string& propertyName);

  std::string MakeXmlReference(const std::string& ns, const std::string& type, const std::string& propertyName);

  using Converter = std::string(Formatter::*)(const std::string& ns, const std::string& typeName, const std::string& propName);
  std::string ResolveReferences(std::string sane, Converter converter);

  std::string typeToMarkdown(std::string_view ns, std::string type, bool toCode, std::string urlSuffix = "");

  std::string GetNamespacePrefix(std::string_view ns);

  static std::string_view ToString(winmd::reader::ElementType elementType);
  std::string ToString(const winmd::reader::coded_index<winmd::reader::TypeDefOrRef>& tdr, bool toCode = true);


  std::string GetType(const winmd::reader::TypeSig& type);
  std::string GetType(const winmd::reader::TypeSig::value_type& valueType);

private:
  Program* program;
};

std::string code(std::string_view v);
std::string link(std::string_view n);

