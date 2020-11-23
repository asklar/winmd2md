// winmd2markdown.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <winmd_reader.h>
#include <boost/algorithm/string/replace.hpp>
#include "output.h"
#include "Options.h"
#include "Format.h"
using namespace std;
using namespace winmd::reader;

string currentNamespace;
string_view ObjectClassName = "Object"; // corresponds to IInspectable in C++/WinRT
string_view ctorName = ".ctor";

std::unique_ptr<winmd::reader::cache> g_cache{ nullptr };

bool hasAttribute(const pair<CustomAttribute, CustomAttribute>& attrs, string attr) {
  for (auto const& ca : attrs) {
    auto const& tnn = ca.TypeNamespaceAndName();
    if (tnn.second == attr) {
      return true;
    }
  }
  return false;
}

//bool IsBuiltInType(string_view n) {
//
//  constexpr std::vector<string> builtInTypes = {
//    "void",
//    "bool",
//    "int????",
//    "int8_t",
//    "short",
//    "int",
//    "int64_t",
//    "uint8_t",
//    "uint16_t",
//    "uint32_t",
//    "uint64_t",
//    "float",
//    "double",
//    "string",
//  };
//
//  return std::find(builtInTypes.begin(), builtInTypes.end(), n);
//}

string link(string_view n) {
  //if (IsBuiltInType(n)) return string(n);
  return "- [" + code(n) + "](" + string(n) + ")";
}

template<typename T> string GetContentAttributeValue(string attrname, const T& t)
{
  for (auto const& ca : t.CustomAttribute()) {
    const auto tnn = ca.TypeNamespaceAndName();
    const auto customAttrName = tnn.second;
    if (tnn.second == attrname) {
      auto const doc = ca.Value();
      for (const auto& arg : doc.NamedArgs()) {
        auto const argname = arg.name;
        if (argname == "Content") {
          auto const argvalue = arg.value;
          auto const& elemSig = std::get<ElemSig>(argvalue.value);
          const string val{ std::get<string_view>(elemSig.value) };

          auto ret = boost::replace_all_copy(val, "\\n", "<br/>");
          return ret;
        }
      }
    }
  }
  return {};
}

string MakeMarkdownReference(string type, string propertyName) {
  string anchor = type;
  string link = type;
  if (!propertyName.empty()) {
    anchor += (!type.empty() ? "." : "") + propertyName;
    if (propertyName == "Properties") {
      // special case a property or method that might be named Properties, in this case we usually want to it and not the Properties section
      propertyName = "Properties-1";
    }
    std::transform(propertyName.begin(), propertyName.end(), propertyName.begin(), ::tolower);
    link += "#" + propertyName;
  }
  return "[" + code(anchor) + "](" + link + ")";
}

string MakeXmlReference(string type, string propertyName) {
  return R"(<see cref=")" + type + ((!type.empty() && !propertyName.empty()) ? "." : "") + propertyName + R"("/>)";
}

bool isIdentifierChar(char x) {
  return isalnum(x) || x == '_' || x == '.';
}

template<typename Converter>
string ResolveReferences(string sane, Converter converter) {
  stringstream ss;

  for (size_t input = 0; input < sane.length(); input++) {
    if (sane[input] == '@') {
      auto firstNonIdentifier = std::find_if(sane.begin() + input + 1, sane.end(), [](auto& x) { return !isIdentifierChar(x); });
      if (firstNonIdentifier != sane.begin() && *(firstNonIdentifier - 1) == '.') {
        firstNonIdentifier--;
      }
      auto next = firstNonIdentifier - sane.begin() - 1;
      auto reference = sane.substr(input + 1, next - input);

      const auto dot = reference.find('.');
      const string type = reference.substr(0, dot);
      const string propertyName = (dot != -1) ? reference.substr(dot + 1) : "";

      ss << converter(type, propertyName);

      input = next;
      continue;
    }
    ss << sane[input];
  }


  return ss.str();
}

template<typename T>
string GetDocString(const T& t) {
  string val = GetContentAttributeValue("DocStringAttribute", t);
  auto sane = boost::replace_all_copy(val, "\\n", "<br/>");
  boost::replace_all(sane, "\r\n", "\n");
  boost::replace_all(sane, "/-/", "//");

  return sane;
}

template<typename T>
string GetDocDefault(const T& t) {
  string val = GetContentAttributeValue("DocDefaultAttribute", t);
  if (val.empty()) return val;
  auto ret = code(val);
  return ret;
}


template<typename T> output& operator<<(output& o, const T& t)
{
  o.currentFile << t;
  return o;
}


template<typename T>
bool IsExperimental(const T& type)
{
  return hasAttribute(type.CustomAttribute(), "ExperimentalAttribute");
}

template<typename T, typename Converter>
string GetDeprecated(const T& type, Converter converter)
{
  for (auto const& ca : type.CustomAttribute()) {
    const auto tnn = ca.TypeNamespaceAndName();
    const auto customAttrName = tnn.second;
    if (tnn.second == "DeprecatedAttribute") {
      auto const depr = ca.Value();
      const std::vector<winmd::reader::FixedArgSig>& args = depr.FixedArgs();
      auto const argvalue = args[0].value;
      auto const& elemSig = std::get<ElemSig>(argvalue);
      const string val{ std::get<string_view>(elemSig.value) };
      return ResolveReferences(val, converter);
    }
  }
  return {};
}

template<typename IT>
bool shouldSkipInterface(const IT /*TypeDef*/& interfaceEntry) {
#ifdef DEBUG
  auto iname = interfaceEntry.TypeName();
#endif
  if (!g_opts->outputExperimental && IsExperimental(interfaceEntry)) return true;

  return hasAttribute(interfaceEntry.CustomAttribute(), "StaticAttribute") ||
    hasAttribute(interfaceEntry.CustomAttribute(), "ExclusiveToAttribute");
}

/// <summary>
/// Prints information that may be missing, e.g. description, whether the type is experimental or not, whether it's deprecated
/// For properties, custom attributes can live in the getter/setter method instead of the property itself.
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="ss"></param>
/// <param name="type"></param>
/// <param name="fallback_type"></param>
template<typename T, typename F = nullptr_t>
void PrintOptionalSections(MemberType mt, output& ss, const T& type, std::optional<F> fallback_type = std::nullopt)
{
  if (IsExperimental(type)) {
    ss << "> **EXPERIMENTAL**\n\n";
  }
  auto depr = GetDeprecated(type, MakeMarkdownReference);
  constexpr bool isProperty = !std::is_same<F, nullptr_t>();
  if constexpr (isProperty)
  {
    if (depr.empty()) depr = GetDeprecated(fallback_type.value(), MakeMarkdownReference);
  }

  if (!depr.empty()) {
    ss << "> **Deprecated**: " << depr << "\n\n";
  }

  auto default_val = GetDocDefault(type);
  if (!default_val.empty()) {
    ss << "**Default value**: " << default_val << "\n\n";
  }
  auto const doc = GetDocString(type);
  if (!doc.empty()) {
    ss << ResolveReferences(doc, MakeMarkdownReference) << "\n\n";

    string name;
    if constexpr (std::is_same<T, TypeDef>()) {
      name = type.TypeName();
    }
    else {
      name = string(type.Parent().TypeName()) + "." + string(type.Name());
    }
    ss.currentXml.AddMember(mt, name, ResolveReferences(doc, MakeXmlReference));
  }
}

std::string_view ToString(ElementType elementType) {
  switch (elementType) {
  case ElementType::Boolean:
    return "bool";
  case ElementType::I:
    return "int????";
  case ElementType::I1:
    return "int8_t";
  case ElementType::I2:
    return "short";
  case ElementType::I4:
    return "int";
  case ElementType::I8:
    return "int64_t";
  case ElementType::U1:
    return "uint8_t";
  case ElementType::U2:
    return "uint16_t";
  case ElementType::U4:
    return "uint32_t";
  case ElementType::U8:
    return "uint64_t";
  case ElementType::R4:
    return "float";
  case ElementType::R8:
    return "double";
  case ElementType::String:
    return "string";
  case ElementType::Class:
    return "{class}";
  case ElementType::GenericInst:
    return "{generic}";
  case ElementType::ValueType:
    return "{ValueType}";
  case ElementType::Object:
    return ObjectClassName;
  default:
    cout << std::hex << (int)elementType << endl;
    return "{type}";

  }
}

std::string GetTypeKind(const TypeDef& type)
{
  if (type.Flags().Semantics() == TypeSemantics::Interface) {
    return "interface";
  }
  else if (type.is_enum()) {
    return "enum";
  }
  else {
    return "class";
  }
}

string GetNamespacePrefix(std::string_view ns)
{
  if (ns == currentNamespace) return "";
  else return string(ns) + ".";
}

std::string typeToMarkdown(std::string_view ns, std::string type, bool toCode, string urlSuffix = "")
{
  constexpr std::string_view docs_msft_com_namespaces[] = {
    "Windows.",
    "Microsoft.",
  };
  string code = toCode ? "`" : "";
  if (ns.empty()) return type; // basic type
  else if (ns == currentNamespace) {
    return "[" + code + type + code + "](" + type + ")";
  }
  else {
    for (const auto& ns_prefix : docs_msft_com_namespaces) {
      if (ns._Starts_with(ns_prefix)) {
        // if it is a Windows type use MSDN, e.g.
        // https://docs.microsoft.com/uwp/api/Windows.UI.Xaml.Automation.ExpandCollapseState
        const std::string docsURL = "https://docs.microsoft.com/uwp/api/";
        return "[" + code + type + code + "](" + docsURL + string(ns) + "." + type + urlSuffix + ")";
      }
    }
    return GetNamespacePrefix(ns) + type;
  }
}

string GetType(const TypeSig& type);


string ToString(const coded_index<TypeDefOrRef>& tdr, bool toCode = true)
{
  if (!tdr) return {};
  if (tdr) {
    switch (tdr.type()) {
    case TypeDefOrRef::TypeDef:
    {
      const auto& td = tdr.TypeDef();
      return typeToMarkdown(td.TypeNamespace(), string(td.TypeName()), toCode);
    }
    case TypeDefOrRef::TypeRef:
    {
      const auto& tr = tdr.TypeRef();
      return typeToMarkdown(tr.TypeNamespace(), string(tr.TypeName()), toCode);
    }
    case TypeDefOrRef::TypeSpec:
    {
      const auto& ts = tdr.TypeSpec();
      const auto& n = ts.Signature();
      const auto& s = n.GenericTypeInst();
      const auto& p = ToString(s.GenericType());
      const auto& ac = s.GenericArgCount();
      if (ac != 1) {
        cout << "mmmm..." << endl;
      }
      for (auto const& a : s.GenericArgs())
      {
        auto x = GetType(a);
        return x;
      }

      return "TypeSpec";
    }
    default:
      throw std::invalid_argument("");
    }
  }
  else {
    return {};
  }
}


string GetType(const TypeSig::value_type& valueType)
{
  switch (valueType.index())
  {
  case 0: // ElementType
    break;
  case 1: // coded_index<TypeDefOrRef>
  {
    const auto& t = std::get<coded_index<TypeDefOrRef>>(valueType);
    return string(ToString(t));
  }
  case 2: // GenericTypeIndex
    return "(generic)";
    break;
  case 3: // GenericTypeInstSig
  {
    const auto& gt = std::get<GenericTypeInstSig>(valueType);
    const auto& genericType = gt.GenericType();
    const auto& outerType = std::string(ToString(genericType, false));
    stringstream ss;
    const auto& prettyOuterType = outerType.substr(1, outerType.find('`') - 1);
    ss << typeToMarkdown(genericType.TypeRef().TypeNamespace(), prettyOuterType, true, "-" + std::to_string(gt.GenericArgCount())) << '<';

    bool first = true;

    for (const auto& arg : gt.GenericArgs()) {
      if (!first) {
        ss << ", ";
      }
      first = false;
      ss << GetType(arg);
    }

    if (winmd::reader::empty(gt.GenericArgs()) && gt.GenericArgCount() != 0) {
      // Missing how to figure out the T in IGeneric<T>
      // This indicates that we relied on a temporary that got deleted when chaining several calls
      throw std::invalid_argument("you found a bug - we probably deleted an object we shouldn't (when doing a.b().c().d())");
    }
    ss << '>';
    return ss.str();
  }
  case 4: // GenericMethodTypeIndex
    break;

  default:
    break;
  }

  return "{NYI}some type";
}

string GetType(const TypeSig& type) {
  if (type.element_type() != ElementType::Class &&
    type.element_type() != ElementType::ValueType &&
    type.element_type() != ElementType::GenericInst
    ) {
    return string(ToString(type.element_type()));
  }
  else {
    return GetType(type.Type());
  }
}


void process_enum(output& ss, const TypeDef& type) {
  auto t = ss.StartType(type.TypeName(), "enum");
  PrintOptionalSections(MemberType::Type, ss, type);

  ss << "| Name |  Value | Description |\n" << "|--|--|--|\n";
  for (auto const& value : type.FieldList()) {
    if (value.Flags().SpecialName()) {
      continue;
    }
    uint32_t val = static_cast<uint32_t>((value.Signature().Type().element_type() == ElementType::U4) ? value.Constant().ValueUInt32() : value.Constant().ValueInt32());
    ss << "|" << code(value.Name()) << " | " << std::hex << "0x" << val << "  |  " << GetDocString(value) << "|\n";
  }
}

MethodDef find_method(const TypeDef& type, string name) {
  for (const auto& m : type.MethodList()) {
    if (m.Name() == name) {
      return m;
    }
  }
  return MethodDef();
}
DEFINE_ENUM_FLAG_OPERATORS(MemberAccess);


void process_property(output& ss, const Property& prop) {
  const auto& type = GetType(prop.Type().Type());
  const auto& name = code(prop.Name());

  const auto& owningType = prop.Parent();
  const auto propName = string(prop.Name());
  const auto& getter = find_method(owningType, "get_" + propName);
  const auto& setter = find_method(owningType, "put_" + propName);
  bool isStatic{ false };

  if ((getter && getter.Flags().Static()) || (setter && setter.Flags().Static())) {
    isStatic = true;
  }

  bool readonly{ false };
  if (!setter || (setter.Flags().Access() & MemberAccess::Public) != MemberAccess::Public) {
    readonly = true;
  }

  auto default_val = GetDocDefault(prop);
  auto cppAttrs = (isStatic ? (code("static") + "   ") : "") + (readonly ? (code("readonly") + " ") : "");
  if (g_opts->propertiesAsTable) {
    auto description = GetDocString(prop);
    if (!default_val.empty()) {
      description += "<br/>default: " + default_val;
    }
    ss << "| " << cppAttrs << "| " << name << " | " << type << " | " << description << " | \n";
  }
  else {
    auto sec = ss.StartSection(propName);
    ss << cppAttrs << " " << type << " " << name << "\n\n";
    PrintOptionalSections(MemberType::Property, ss, prop, std::make_optional(getter));

  }
}

void process_method(output& ss, const MethodDef& method, string_view realName = "") {
  std::string returnType;
  const auto& signature = method.Signature();
  if (realName.empty()) {
    if (method.Signature().ReturnType()) {
      const auto& sig = method.Signature();
      const auto& rt = sig.ReturnType();
      const auto& type = rt.Type();
      returnType = GetType(type);
    }
    else {
      returnType = "void";
    }
  }
  const auto& flags = method.Flags();
  const string_view name = realName.empty() ? method.Name() : realName;
  stringstream sstr;
  sstr << (flags.Static() ? (code("static") + " ") : "")
    //    << (flags.Abstract() ? "abstract " : "")
    << returnType << " **" << code(name) << "**(";

  int i = 0;

  std::vector<string_view> paramNames;
  for (auto const& p : method.ParamList()) {
    paramNames.push_back(p.Name());
  }
  constexpr auto resultParamName = "result";
  if (!paramNames.empty() && paramNames[0] == resultParamName) {
    paramNames.erase(paramNames.begin());
  }

  for (const auto& param : signature.Params()) {
    if (i != 0) {
      sstr << ", ";
    }

    const auto out = param.ByRef() ? "**out** " : "";
    sstr << out << GetType(param.Type()) << " " << paramNames[i];
    i++;
  }
  sstr << ")";
  const std::string method_name = string{ name };
  auto st = ss.StartSection(method_name);
  ss << sstr.str() << "\n\n";

  PrintOptionalSections(MemberType::Method, ss, method);
  ss << "\n\n";
}


void process_field(output& ss, const Field& field) {
  const auto& type = GetType(field.Signature().Type());
  const auto& name = string(field.Name());
  if (g_opts->fieldsAsTable) {
    auto description = GetDocString(field);
    ss << "| " << name << " | " << type << " | " << description << " |\n";
  }
  else {
    auto sec = ss.StartSection(name);
    auto s = field.Signature();
    auto st = s.Type();
    auto tt = st.Type();
    auto et = std::get_if<ElementType>(&tt);
    string typeStr{};
    if (et) {
      typeStr = code(type);
    }
    else {
      typeStr = GetType(tt);
    }
    ss << "Type: " << typeStr << "\n\n";
    PrintOptionalSections(MemberType::Field, ss, field);
  }
}

void process_struct(output& ss, const TypeDef& type) {
  const auto t = ss.StartType(type.TypeName(), "struct");
  PrintOptionalSections(MemberType::Type, ss, type);

  const auto fs = ss.StartSection("Fields");

  using entry_t = pair<string_view, const Field>;
  std::list<entry_t> sorted;
  for (auto const& field : type.FieldList()) {
    sorted.push_back(make_pair<string_view, const Field>(field.Name(), Field(field)));
  }
  sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });
  if (g_opts->fieldsAsTable) {
    ss << "| Name | Type | Description |" << "\n" << "|---|---|---|" << "\n";
  }
  for (auto const& field : sorted) {
    if (!g_opts->outputExperimental && IsExperimental(field.second)) continue;
    process_field(ss, field.second);
  }
}

void process_delegate(output& ss, const TypeDef& type) {
  const auto t = ss.StartType(type.TypeName(), "delegate");
  PrintOptionalSections(MemberType::Type, ss, type);
  for (auto const& method : type.MethodList()) {
    constexpr auto invokeName = "Invoke";
    const auto& name = method.Name();
    if (method.SpecialName() && name == invokeName) {
      if (!g_opts->outputExperimental && IsExperimental(method)) continue;
      process_method(ss, method);
    }
  }
}

map<string, vector<TypeDef>> interfaceImplementations{};


void process_class(output& ss, const TypeDef& type, string kind) {
  const auto& className = string(type.TypeName());
  const auto t = ss.StartType(className, kind);

  const auto& extends = ToString(type.Extends());
  if (!extends.empty() && extends != "System.Object") {
    ss << "Extends: " + extends << "\n\n";
  }
  if (kind == "interface" && interfaceImplementations.find(className) != interfaceImplementations.end())
  {
    ss << "Implemented by: \n";
    for (auto const& imp : interfaceImplementations[className])
    {
      ss << "- " << typeToMarkdown(imp.TypeNamespace(), string(imp.TypeName()), true) << "\n";
    }
  }
  {
    int i = 0;
    for (auto const& ii : type.InterfaceImpl()) {
      const auto& tr = ii.Interface().TypeRef();
      const auto& ifaceName = string(tr.TypeName());

      const auto& td = g_cache->find(tr.TypeNamespace(), tr.TypeName());
      if (shouldSkipInterface(td)) continue;

      if (i == 0) {
        ss << "Implements: ";
      }
      else {
        ss << ", ";
      }
      i++;
      ss << ToString(ii.Interface());
      interfaceImplementations[ifaceName].push_back(TypeDef(type));
    }
    ss << "\n\n";
  }
  PrintOptionalSections(MemberType::Type, ss, type);

  {
    using entry_t = pair<string_view, const Property>;
    std::list<entry_t> sorted;
    for (auto const& prop : type.PropertyList()) {
      if (!g_opts->outputExperimental && IsExperimental(prop)) continue;
      sorted.push_back(make_pair<string_view, const Property>(prop.Name(), Property(prop)));
    }
    sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });
    if (!sorted.empty()) {
      auto ps = ss.StartSection("Properties");
      if (g_opts->propertiesAsTable) {
        ss << "|   | Name|Type|Description|" << "\n"
          << "|---|-----|----|-----------|" << "\n";
      }
      for (auto const& prop : sorted) {
        process_property(ss, prop.second);
      }
    }
  }
  ss << "\n";
  {

    using entry_t = pair<string_view, const MethodDef>;
    std::list<entry_t> sorted;
    for (auto const& method : type.MethodList()) {
      if (!g_opts->outputExperimental && IsExperimental(method)) continue;
      sorted.push_back(make_pair<string_view, const MethodDef>(method.Name(), MethodDef(method)));
    }
    sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });

    if (std::find_if(sorted.begin(), sorted.end(), [](auto const& x) { return x.first == ctorName; }) != sorted.end())
    {
      auto ms = ss.StartSection("Constructors");
      for (auto const& method : sorted) {
        if (method.second.SpecialName() && (method.first == ctorName)) {
          process_method(ss, method.second, type.TypeName());
        }
        else {
          continue;
        }
      }
    }
    ss << "\n";
    if (std::find_if(sorted.begin(), sorted.end(), [](auto const& x) { return !x.second.SpecialName(); }) != sorted.end())
    {
      auto ms = ss.StartSection("Methods");
      for (auto const& method : sorted) {
        if (method.second.SpecialName()) {
#ifdef DEBUG
          std::cout << "Skipping special method: " << string(method.second.Name()) << "\n";
#endif
          continue; // get_ / put_ methods that are properties
        }
        else {
          process_method(ss, method.second);
        }
      }
    }
  }

  ss << "\n";
  {
    using entry_t = pair<string_view, const Event>;
    std::list<entry_t> sorted;
    for (auto const& evt : type.EventList()) {
      if (!g_opts->outputExperimental && IsExperimental(evt)) continue;
      sorted.push_back(make_pair<string_view, const Event>(evt.Name(), Event(evt)));
    }
    sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });

    if (!sorted.empty()) {
      auto es = ss.StartSection("Events");
      for (auto const& evt : sorted) {
        auto n = evt.first;
        auto ees = ss.StartSection("`" + string(evt.first) + "`");
        ss << "Type: " << ToString(evt.second.EventType()) << "\n";
      }
    }
  }
}


void write_index(string_view namespaceName, const cache::namespace_members& ns) {
  ofstream index("out/index" + g_opts->fileSuffix + ".md");

  index << R"(---
id: Native-API-Reference
title: namespace )" << namespaceName << R"(
sidebar_label: Full reference
---

)";

  index << "## Enums" << "\n";
  for (auto const& t : ns.enums) {
    if (!g_opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Interfaces" << "\n";
  for (auto const& t : ns.interfaces) {
    if (!g_opts->outputExperimental && IsExperimental(t)) continue;
    if (shouldSkipInterface(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Structs" << "\n";
  for (auto const& t : ns.structs) {
    if (!g_opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Classes" << "\n";

  for (auto const& t : ns.classes) {
    if (!g_opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Delegates" << "\n";
  for (auto const& t : ns.delegates) {
    if (!g_opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }
}

void process(output& ss, string_view namespaceName, const cache::namespace_members& ns) {
  ss.StartNamespace(namespaceName);
  for (auto const& enumEntry : ns.enums) {
    if (!g_opts->outputExperimental && IsExperimental(enumEntry)) continue;
    process_enum(ss, enumEntry);
  }

  for (auto const& classEntry : ns.classes) {
    if (!g_opts->outputExperimental && IsExperimental(classEntry)) continue;
    process_class(ss, classEntry, "class");
  }

  for (auto const& interfaceEntry : ns.interfaces) {
    if (!shouldSkipInterface(interfaceEntry)) {
      process_class(ss, interfaceEntry, "interface");
    }
  }

  for (auto const& structEntry : ns.structs) {
    if (!g_opts->outputExperimental && IsExperimental(structEntry)) continue;
    process_struct(ss, structEntry);
  }


  for (auto const& delegateEntry : ns.delegates) {
    if (!g_opts->outputExperimental && IsExperimental(delegateEntry)) continue;
    process_delegate(ss, delegateEntry);
  }

  write_index(namespaceName, ns);
}

string getWindowsWinMd() {
  // The root location for Windows SDKs is stored in HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Kits\Installed Roots
  // under KitsRoot10
  // But it should be ok to check the default path for most cases

  const filesystem::path sdkRoot = "C:\\Program Files (x86)\\Windows Kits\\10\\UnionMetadata";
  if (!g_opts->sdkVersion.empty()) {
    return (sdkRoot / g_opts->sdkVersion / "Windows.winmd").u8string();
  }

  for (const auto& d : filesystem::directory_iterator(sdkRoot)) {
    auto dirPath = d.path();
    filesystem::path winmd = dirPath / "Windows.winmd";
    if (filesystem::exists(winmd)) {
      return winmd.u8string();
    }
  }

  throw std::invalid_argument("Couldn't find Windows.winmd");
}

void PrintHelp(string name) {
  cerr << "https://github.com/asklar/winmd2md" << "\n";
  cerr << "Usage: " << name << " [options] pathToMetadata.winmd\n\n";
  cerr << "Options:\n";
  for (const auto& o : get_option_names()) {
    cerr << "\t" << setw(14) << "/" << o.name << "\t" << o.description << "\n";
  }
}
int main(int argc, char** argv)
{
  if (argc < 2) {
    PrintHelp(argv[0]);
    return -1;
  }
  g_opts = new options(std::vector<string>(argv + 1, argv + argc));
  if (g_opts->help) {
    PrintHelp(argv[0]);
    return 0;
  }

  auto windows_winmd = getWindowsWinMd();

  std::vector<string> files = {
    windows_winmd,
    g_opts->winMDPath,
  };
  g_cache = std::make_unique<cache>(files);

  output o;
  // ostream& ss = cout;
  for (auto const& namespaceEntry : g_cache->namespaces()) {
    if (namespaceEntry.first._Starts_with("Windows.")) continue;
    filesystem::path nsPath(namespaceEntry.first);
    filesystem::create_directory(nsPath);
    filesystem::current_path(nsPath);
    currentNamespace = namespaceEntry.first;
    filesystem::current_path("..");
    process(o, namespaceEntry.first, namespaceEntry.second);
  }
  delete g_opts;
  return 0;
}
