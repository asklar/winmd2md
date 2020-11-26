#pragma once
#include <string>
#include <filesystem>
#include <winmd_reader.h>

#include "Options.h"
#include "output.h"

struct Program {
  std::string currentNamespace;
  static constexpr std::string_view ObjectClassName = "Object"; // corresponds to IInspectable in C++/WinRT
  static constexpr std::string_view ctorName = ".ctor";
  std::unique_ptr<winmd::reader::cache> cache{ nullptr };
  std::unique_ptr<options> opts;

  std::map<std::string, std::vector<winmd::reader::TypeDef>> interfaceImplementations{};


  int Process(std::vector<std::string>);

  template <typename T>
  void AddUniqueReference(const T& type, const winmd::reader::TypeDef& owningType);

  // map of namespaces N -> (map of types T in N -> (list of types that reference T))
  std::map<std::string, std::map<std::string, std::vector<winmd::reader::TypeDef>>> references{};
  output ss;
  friend class UnitTests;

  Program() : ss(this) {}
private:
  void process_class(output& ss, const winmd::reader::TypeDef& type, std::string kind);
  void process_enum(output& ss, const winmd::reader::TypeDef& type);
  void process_property(output& ss, const winmd::reader::Property& prop);
  void process_method(output& ss, const winmd::reader::MethodDef& method, std::string_view realName = "");
  void process_field(output& ss, const winmd::reader::Field& field);
  void process_struct(output& ss, const winmd::reader::TypeDef& type);
  void process_delegate(output& ss, const winmd::reader::TypeDef& type);
  void process(std::string_view namespaceName, const winmd::reader::cache::namespace_members& ns);

  void write_index(std::string_view namespaceName, const winmd::reader::cache::namespace_members& ns);

  void AddReference(const winmd::reader::TypeSig& prop, const winmd::reader::TypeDef& owningType);
  void AddReference(const winmd::reader::coded_index<winmd::reader::TypeDefOrRef>& classTypeDefOrRef, const winmd::reader::TypeDef& owningType);

  std::string getWindowsWinMd();

};

extern Program g_program;

