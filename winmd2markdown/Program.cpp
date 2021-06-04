#include <string_view>
#include <sstream>
#include <boost/algorithm/string/replace.hpp>

#include "Program.h"
#include "Options.h"
#include "Format.h"

using namespace winmd::reader;
using namespace std;


bool hasAttribute(const pair<CustomAttribute, CustomAttribute>& attrs, string attr) {
  for (auto const& ca : attrs) {
    auto const& tnn = ca.TypeNamespaceAndName();
    if (tnn.second == attr) {
      return true;
    }
  }
  return false;
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

          auto ret = boost::replace_all_copy(val, "\\n", "\n");
          return ret;
        }
      }
    }
  }
  return {};
}


template<typename T>
string GetDocString(const T& t) {
  string val = GetContentAttributeValue("DocStringAttribute", t);
  auto sane = boost::replace_all_copy(val, "\\n", "\n");
  boost::replace_all(sane, "\\r", "\r");
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
  *(o.currentFile) << t;
  return o;
}


template<typename T>
bool IsExperimental(const T& type)
{
  return hasAttribute(type.CustomAttribute(), "ExperimentalAttribute");
}

template<typename T, typename Converter>
string Program::GetDeprecated(const T& type, Converter converter)
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
      return format.ResolveReferences(val, converter);
    }
  }
  return {};
}

template<typename IT>
bool Program::shouldSkipInterface(const IT /*TypeDef*/& interfaceEntry) {
#ifdef DEBUG
  auto iname = interfaceEntry.TypeName();
#endif
  if (!opts->outputExperimental && IsExperimental(interfaceEntry)) return true;

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
void Program::PrintOptionalSections(MemberType mt, output& ss, const T& type, std::optional<F> fallback_type)
{
  if (IsExperimental(type)) {
    ss << "> **EXPERIMENTAL**\n\n";
  }
  auto depr = GetDeprecated(type, &Formatter::MakeMarkdownReference);
  constexpr bool isProperty = !std::is_same<F, nullptr_t>();
  if constexpr (isProperty)
  {
    if (depr.empty()) depr = GetDeprecated(fallback_type.value(), &Formatter::MakeMarkdownReference);
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
    ss << format.ResolveReferences(doc, &Formatter::MakeMarkdownReference) << "\n\n";

    string name;
    if constexpr (std::is_same<T, TypeDef>()) {
      name = type.TypeName();
    }
    else {
      name = string(type.Parent().TypeName()) + "." + string(type.Name());
    }
    ss.currentXml.AddMember(mt, name, format.ResolveReferences(doc, &Formatter::MakeXmlReference));
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


MethodDef find_method(const TypeDef& type, string name) {
  for (const auto& m : type.MethodList()) {
    if (m.Name() == name) {
      return m;
    }
  }
  return MethodDef();
}
DEFINE_ENUM_FLAG_OPERATORS(MemberAccess);







void Program::process(std::string_view namespaceName, const cache::namespace_members& ns) {
  ss.StartNamespace(namespaceName);
  for (auto const& enumEntry : ns.enums) {
    if (!opts->outputExperimental && IsExperimental(enumEntry)) continue;
    process_enum(ss, enumEntry);
  }

  for (auto const& classEntry : ns.classes) {
    if (!opts->outputExperimental && IsExperimental(classEntry)) continue;
    process_class(ss, classEntry, "class");
  }

  for (auto const& interfaceEntry : ns.interfaces) {
    if (!shouldSkipInterface(interfaceEntry)) {
      process_class(ss, interfaceEntry, "interface");
    }
  }

  for (auto const& structEntry : ns.structs) {
    if (!opts->outputExperimental && IsExperimental(structEntry)) continue;
    process_struct(ss, structEntry);
  }


  for (auto const& delegateEntry : ns.delegates) {
    if (!opts->outputExperimental && IsExperimental(delegateEntry)) continue;
    process_delegate(ss, delegateEntry);
  }

  write_index(namespaceName, ns);

  if (opts->printReferenceGraph) std::cout << "Reference graph:\n";
  for (const auto& backReference : references[string(namespaceName)]) {
    std::ofstream md(ss.GetFileForType(backReference.first), std::ofstream::out | std::ofstream::app);
    if (opts->printReferenceGraph) std::cout << backReference.first << " <-- ";
    md << R"(

## Referenced by
)";
    std::vector<std::string> sorted;
    std::for_each(backReference.second.begin(), backReference.second.end(), [&sorted](auto& x) { sorted.push_back(string(x.TypeName())); });
    std::sort(sorted.begin(), sorted.end());
    for (const auto& i : sorted) {
      md << link(i) << "\n";
      if (opts->printReferenceGraph) std::cout << i << "  ";
    }
    if (opts->printReferenceGraph) std::cout << "\n";
  }
}

void Program::process_class(output& ss, const TypeDef& type, string kind) {
  const auto& className = string(type.TypeName());
  const auto t = ss.StartType(className, kind);

  const auto& extends = format.ToString(type.Extends());
  if (!extends.empty() && extends != "System.Object") {
    ss << "Extends: " + extends << "\n\n";
  }

  if (kind == "interface" && interfaceImplementations.find(className) != interfaceImplementations.end())
  {
    ss << "Implemented by: \n";
    for (auto const& imp : interfaceImplementations[className])
    {
      ss << "- " << format.typeToMarkdown(imp.TypeNamespace(), string(imp.TypeName()), true) << "\n";
    }
  }

  // Print interface implementations
  {
    int i = 0;
    for (auto const& ii : type.InterfaceImpl()) {
      const auto& tr = ii.Interface().TypeRef();
      const auto& ifaceName = string(tr.TypeName());

      const auto& td = cache->find(tr.TypeNamespace(), tr.TypeName());
      if (shouldSkipInterface(td)) continue;

      if (i == 0) {
        ss << "Implements: ";
      }
      else {
        ss << ", ";
      }
      i++;
      ss << format.ToString(ii.Interface());
      interfaceImplementations[ifaceName].push_back(TypeDef(type));
    }
    ss << "\n\n";
  }
  PrintOptionalSections(MemberType::Type, ss, type);

  // Print properties
  {
    using entry_t = pair<string_view, const Property>;
    std::list<entry_t> sorted;
    for (auto const& prop : type.PropertyList()) {
      if (!opts->outputExperimental && IsExperimental(prop)) continue;
      sorted.push_back(make_pair<string_view, const Property>(prop.Name(), Property(prop)));
    }
    sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });
    if (!sorted.empty()) {
      auto ps = ss.StartSection("Properties");
      if (opts->propertiesAsTable) {
        ss << "|   | Name|Type|Description|" << "\n"
          << "|---|-----|----|-----------|" << "\n";
      }
      for (auto const& prop : sorted) {
        process_property(ss, prop.second);
      }
    }
  }
  ss << "\n";

  // Print methods and constructors
  {

    using entry_t = pair<string_view, const MethodDef>;
    std::list<entry_t> sorted;
    for (auto const& method : type.MethodList()) {
      if (!opts->outputExperimental && IsExperimental(method)) continue;
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
  // Print events
  {
    using entry_t = pair<string_view, const Event>;
    std::list<entry_t> sorted;
    for (auto const& evt : type.EventList()) {
      if (!opts->outputExperimental && IsExperimental(evt)) continue;
      sorted.push_back(make_pair<string_view, const Event>(evt.Name(), Event(evt)));
    }
    sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });

    if (!sorted.empty()) {
      auto es = ss.StartSection("Events");
      for (auto const& evt : sorted) {
        auto n = evt.first;
        auto ees = ss.StartSection("`" + string(evt.first) + "`");
        ss << "Type: " << format.ToString(evt.second.EventType()) << "\n";
        AddReference(evt.second.EventType(), type);
      }
    }
  }
}


template <typename T>
void Program::AddUniqueReference(const T& type, const TypeDef& owningType)
{
  if (type.TypeNamespace() == owningType.TypeNamespace() && type.TypeName() == owningType.TypeName()) return;
  auto& vec = references[string(type.TypeNamespace())][string(type.TypeName())];
  if (std::find(vec.cbegin(), vec.cend(), owningType) == vec.cend()) {
    vec.push_back(owningType);
  }
}

void Program::AddReference(const coded_index<TypeDefOrRef>& classTypeDefOrRef, const TypeDef& owningType) {
  switch (classTypeDefOrRef.type()) {
  case TypeDefOrRef::TypeRef: {
    auto type = classTypeDefOrRef.TypeRef();
    AddUniqueReference(type, owningType);
    break;
  }
  case TypeDefOrRef::TypeDef: {
    auto type = classTypeDefOrRef.TypeDef();
    AddUniqueReference(type, owningType);
    break;
  }
  case TypeDefOrRef::TypeSpec: {
    auto type = classTypeDefOrRef.TypeSpec().Signature();
    auto genType = type.GenericTypeInst().GenericType(); // maybe Windows.Foundation.IEventHandler<T> for some T
    AddReference(genType, owningType);
    for (const auto& targ : type.GenericTypeInst().GenericArgs()) {
      AddReference(targ, owningType);
    }
    //AddUniqueReference(type.Signature(), owningType);
    break;
  }
  }
}

void Program::AddReference(const TypeSig& prop, const TypeDef& owningType) {
  switch (prop.element_type()) {
  case ElementType::Class:
  case ElementType::Enum:
  case ElementType::ValueType:
  {
    auto classTypeDefOrRef = std::get<coded_index<TypeDefOrRef>>(prop.Type());
    AddReference(classTypeDefOrRef, owningType);
  }
  default:
    break;
  }
}

void Program::process_property(output& ss, const Property& prop) {
  const auto& type = format.GetType(prop.Type().Type());
  const auto& name = code(prop.Name());

  const auto& owningType = prop.Parent();
  const auto propName = string(prop.Name());
  AddReference(prop.Type().Type(), owningType);
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
  if (opts->propertiesAsTable) {
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

void Program::process_method(output& ss, const MethodDef& method, string_view realName) {
  std::string returnType;
  const auto& signature = method.Signature();
  if (realName.empty()) {
    if (method.Signature().ReturnType()) {
      const auto& sig = method.Signature();
      const auto& rt = sig.ReturnType();
      const auto& type = rt.Type();
      AddReference(type, method.Parent());
      returnType = format.GetType(type);
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
    sstr << out << format.GetType(param.Type()) << " " << paramNames[i];
    AddReference(param.Type(), method.Parent());
    i++;
  }
  sstr << ")";
  const std::string method_name = string{ name };
  auto st = ss.StartSection(method_name);
  ss << sstr.str() << "\n\n";

  PrintOptionalSections(MemberType::Method, ss, method);
  ss << "\n\n";
}


void Program::process_field(output& ss, const Field& field) {
  const auto& type = format.GetType(field.Signature().Type());
  const auto& name = string(field.Name());
  if (opts->fieldsAsTable) {
    auto description = GetDocString(field);
    ss << "| " << name << " | " << type << " | " << description << " |\n";
  }
  else {
    auto sec = ss.StartSection(name);
    auto s = field.Signature();
    auto st = s.Type();
    AddReference(st, field.Parent());
    auto tt = st.Type();
    auto et = std::get_if<ElementType>(&tt);
    string typeStr{};
    if (et) {
      typeStr = code(type);
    }
    else {
      typeStr = format.GetType(tt);
    }
    ss << "Type: " << typeStr << "\n\n";
    PrintOptionalSections(MemberType::Field, ss, field);
  }

}

void Program::process_struct(output& ss, const TypeDef& type) {
  const auto t = ss.StartType(type.TypeName(), "struct");
  PrintOptionalSections(MemberType::Type, ss, type);

  const auto fs = ss.StartSection("Fields");

  using entry_t = pair<string_view, const Field>;
  std::list<entry_t> sorted;
  for (auto const& field : type.FieldList()) {
    sorted.push_back(make_pair<string_view, const Field>(field.Name(), Field(field)));
  }
  sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });
  if (opts->fieldsAsTable) {
    ss << "| Name | Type | Description |" << "\n" << "|---|---|---|" << "\n";
  }
  for (auto const& field : sorted) {
    if (!opts->outputExperimental && IsExperimental(field.second)) continue;
    process_field(ss, field.second);
  }
}

void Program::process_delegate(output& ss, const TypeDef& type) {
  const auto t = ss.StartType(type.TypeName(), "delegate");
  PrintOptionalSections(MemberType::Type, ss, type);
  for (auto const& method : type.MethodList()) {
    constexpr auto invokeName = "Invoke";
    const auto& name = method.Name();
    if (method.SpecialName() && name == invokeName) {
      if (!opts->outputExperimental && IsExperimental(method)) continue;
      process_method(ss, method);
    }
  }
}

void Program::process_enum(output& ss, const TypeDef& type) {
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

void PrintHelp(string name) {
  cerr << "https://github.com/asklar/winmd2md" << "\n";
  cerr << "Usage: " << name << " [options] pathToMetadata.winmd\n\n";
  cerr << "Options:\n";
  for (const auto& o : get_option_names()) {
    cerr << "\t" << setw(14) << "/" << o.name << "\t" << o.description << "\n";
  }
}

int Program::Process(std::vector<std::string> argv) {
  if (argv.empty()) {
    PrintHelp("winmd2markdown.exe");
    return -1;
  }

  opts = std::make_unique<options>(argv);
  if (opts->help) {
    PrintHelp(argv[0]);
    return 0;
  }

  std::vector<string> files = {
    getWindowsWinMd(),
    opts->winMDPath,
  };
  cache = std::make_unique<winmd::reader::cache>(files);

  for (auto const& namespaceEntry : cache->namespaces()) {
    if (namespaceEntry.first._Starts_with("Windows.")) continue;
    filesystem::path nsPath(namespaceEntry.first);
    filesystem::create_directory(nsPath);
    filesystem::current_path(nsPath);
    currentNamespace = namespaceEntry.first;
    filesystem::current_path("..");
    process(namespaceEntry.first, namespaceEntry.second);
  }
  return 0;
}

void Program::write_index(string_view namespaceName, const cache::namespace_members& ns) {
  ofstream index(ss.GetFileForType("index"));

  const auto apiVersionPrefix = (opts->apiVersion != "") ? ("version-" + opts->apiVersion + "-") : "";

  index << R"(---
id: )" << apiVersionPrefix << R"(Native-API-Reference
title: namespace )" << namespaceName << R"(
sidebar_label: Full reference
)";

  if (opts->apiVersion != "") {
    index << "original_id: " << "Native-API-Reference" << "\n";
  }

  index << R"(
---

)";

  index << "## Enums" << "\n";
  for (auto const& t : ns.enums) {
    if (!opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Interfaces" << "\n";
  for (auto const& t : ns.interfaces) {
    if (!opts->outputExperimental && IsExperimental(t)) continue;
    if (shouldSkipInterface(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Structs" << "\n";
  for (auto const& t : ns.structs) {
    if (!opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Classes" << "\n";

  for (auto const& t : ns.classes) {
    if (!opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }

  index << "## Delegates" << "\n";
  for (auto const& t : ns.delegates) {
    if (!opts->outputExperimental && IsExperimental(t)) continue;
    index << link(t.TypeName()) << "\n";
  }
}

string Program::getWindowsWinMd() {
  // The root location for Windows SDKs is stored in HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Kits\Installed Roots
  // under KitsRoot10
  // But it should be ok to check the default path for most cases

  const filesystem::path sdkRoot = "C:\\Program Files (x86)\\Windows Kits\\10\\UnionMetadata";
  if (!opts->sdkVersion.empty()) {
    return (sdkRoot / opts->sdkVersion / "Windows.winmd").u8string();
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
