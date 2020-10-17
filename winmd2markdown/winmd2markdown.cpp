// winmd2markdown.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <winmd_reader.h>
#include <filesystem>
#include <array>
using namespace std;
using namespace winmd::reader;

string currentNamespace;
string_view ObjectClassName = "Object"; // corresponds to IInspectable in C++/WinRT
string_view ctorName = ".ctor";


std::string code(std::string_view v) {
  return "`" + string(v) + "`";
}

struct output
{
private:
  struct type_helper;
  struct section_helper;
public:
  output() {
    std::error_code ec;
    std::filesystem::create_directory("out", ec); // ignore ec
  }
  std::ofstream currentFile;
  type_helper StartType(std::string_view name, std::string_view kind) {
    if (currentFile.is_open()) {
      EndType();
    }
    indents = 0;
    std::filesystem::path out("out");
    currentFile = std::ofstream(out / (std::string(name) + ".md"));
    
    return type_helper(*this, string(kind) + " " + string(name));
  }

  section_helper StartSection(const std::string& a) {
    return section_helper(*this, a);
  }
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
    section_helper(output& out, string s) : o(out) {
      o.indents++;
      string t(o.indents, '#');
      o << t << " " << s << "\n";
    }
    ~section_helper() {
      o.indents--;
    }
  };
  struct type_helper {
    output& o;
    section_helper sh;
    type_helper(output& out, string s) : o(out), sh(o.StartSection(s)) {};
    ~type_helper() {
      o.EndType();
    }
  };
};

template<typename T> output& operator<<(output& o, const T& t)
{
  o.currentFile << t;
  return o;
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
  string code = toCode ? "`" : "";
  if (ns == "") return type; // basic type
  else if (ns == currentNamespace) {
    return "[" + code + type + code + "](" + type + ")";
  }
  else if (ns._Starts_with("Windows.")) {
    // if it is a Windows type use MSDN, e.g.
    // https://docs.microsoft.com/uwp/api/Windows.UI.Xaml.Automation.ExpandCollapseState
    const std::string docsURL = "https://docs.microsoft.com/uwp/api/";
    return "[" + code + type + code + "](" + docsURL + string(ns) + "." + type + urlSuffix + ")";
  }
  return GetNamespacePrefix(ns) + type;
}

string GetType(const TypeSig& type);


string ToString(const coded_index<TypeDefOrRef>& tdr, bool toCode = true)
{
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
      auto n = ts.Signature();
      auto s = n.GenericTypeInst();
      auto p = ToString(s.GenericType());
      auto ac = s.GenericArgCount();
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
    if (gt.GenericArgs().first == gt.GenericArgs().second && gt.GenericArgCount() != 0) {
      auto gat = gt.GenericType();
      auto n = GetType(gat);
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
  for (auto const& value : type.FieldList()) {
    if (value.Flags().SpecialName()) {
      continue;
    }
    uint32_t val = static_cast<uint32_t>((value.Signature().Type().element_type() == ElementType::U4) ? value.Constant().ValueUInt32() : value.Constant().ValueInt32());
    ss << "    " << value.Name() << " = " << std::hex << "0x" << val << "\n";
  }
}


void process_property(output& ss, const Property& prop) {
  ss.StartSection(code(prop.Name()));
  ss << "Type: " << GetType(prop.Type().Type()) << "\n";
}

void process_method(output& ss, const MethodDef& method, string_view realName = "") {
  std::string returnType;
  const auto& signature = method.Signature();
  if (method.Signature().ReturnType()) {
    const auto& type = method.Signature().ReturnType().Type();
    returnType = GetType(type);
  }
  else {
    returnType = "void";
  }
  const auto flags = method.Flags();
  const auto& name = realName.empty() ? method.Name() : realName;
  ss  << (flags.Static() ? code("static ") : "")
//    << (flags.Abstract() ? "abstract " : "")
    << code(returnType) << " **" << code(name) << "**(";

  int i = 0;

  std::vector<string_view> paramNames;
  for (auto const& p : method.ParamList()) {
    paramNames.push_back(p.Name());
  }

  for (const auto& param : signature.Params()) {
    if (i != 0) {
      ss << ", ";
    }
    ss << GetType(param.Type()) << " " << paramNames[i];
    i++;
  }
  ss << ")" << "\n\n";
}


void process_field(output& ss, const Field& field) {
  auto fs = ss.StartSection(string(field.Name()));
  ss << "Type: " << GetType(field.Signature().Type()) << "\n";
}

void process_struct(output& ss, const TypeDef& type) {
  auto t = ss.StartType(type.TypeName(), "struct");
  auto fs = ss.StartSection("Fields");
  for (auto const& field : type.FieldList()) {
    process_field(ss, field);
  }
}

void process_delegate(output& ss, const TypeDef& type) {
  auto t = ss.StartType(type.TypeName(), "delegate");
  for (auto const& method : type.MethodList()) {
    if (method.SpecialName() && method.Name() == "Invoke") {
      process_method(ss, method);
    }
  }
}
void process_class(output& ss, const TypeDef& type, string kind) {
  auto t = ss.StartType(type.TypeName(), kind);
  
  auto extends = ToString(type.Extends());
  if (!extends.empty()) {
    ss << "Extends: " + extends << "\n";
  }

  {
    auto ps = ss.StartSection("Properties");
    for (auto const& prop : type.PropertyList()) {
      process_property(ss, prop);
    }
  }
  ss << "\n";
  {
    auto ms = ss.StartSection("Methods");
    for (auto const& method : type.MethodList()) {
      if (method.Name() == ctorName) {
        process_method(ss, method, type.TypeName());
      }
      else if (method.SpecialName()) {
        std::cout << "Skipping special method: " << string(method.Name()) << "\n";
        continue; // get_ / put_ methods that are properties
      }
      else {
        process_method(ss, method);
      }
    }
  }
  ss << "\n";
  {
    auto es = ss.StartSection("Events");
    for (auto const& evt : type.EventList()) {
      auto n = evt.Name();
      auto ees = ss.StartSection("`" + string(evt.Name()) + "`");
      ss << "Type: " << ToString(evt.EventType()) << "\n";
    }
  }
}

void process(output& ss, const cache::namespace_members& ns) {
  for (auto const& enumEntry : ns.enums) {
    process_enum(ss, enumEntry);
  }

  for (auto const& interfaceEntry : ns.interfaces) {
    process_class(ss, interfaceEntry, "interface");
  }

  for (auto const& structEntry : ns.structs) {
    process_struct(ss, structEntry);
  }

  for (auto const& classEntry : ns.classes) {
    process_class(ss, classEntry, "class");
  }

  for (auto const& delegateEntry : ns.delegates) {
    process_delegate(ss, delegateEntry);
  }
}

int main(int argc, char** argv)
{
  if (argc != 2) {
    cerr << "Usage: " << argv[0] << " pathToMetadata.winmd";
    return -1;
  }

  std::string winmdPath(argv[1]);
  cache cache(winmdPath);
  output o;
  // ostream& ss = cout;
  for (auto const& namespaceEntry : cache.namespaces()) {
    cout << "namespace " << namespaceEntry.first << endl;
    filesystem::path nsPath(namespaceEntry.first);
    filesystem::create_directory(nsPath);
    filesystem::current_path(nsPath);
    currentNamespace = namespaceEntry.first;
    filesystem::current_path("..");
    process(o, namespaceEntry.second);
  }
  return 0;
}
