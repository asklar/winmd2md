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

using namespace std;
using namespace winmd::reader;

string currentNamespace;
string_view ObjectClassName = "Object"; // corresponds to IInspectable in C++/WinRT
string_view ctorName = ".ctor";


std::string code(std::string_view v) {
  return "`" + string(v) + "`";
}

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

template<typename T>
string GetDocString(const T& t) {
  string val = GetContentAttributeValue("DocStringAttribute", t);
  auto ret = boost::replace_all_copy(val, "\\n", "<br/>");
  boost::replace_all(ret, "\r\n", "\n");
  boost::replace_all(ret, "/-/", "//");
  return ret;
}

template<typename T>
string GetDocDefault(const T& t) {
  string val = GetContentAttributeValue("DocDefaultAttribute", t);
  if (val.empty()) return val;
  auto ret = code(val);
  return ret;
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
    const string filename = string(name) + ".md";
    currentFile = std::ofstream(out / filename);
    currentFile << "---\n" <<
      "id: " << name << "\n" <<
      "title: " << name<< "\n" <<
      "---\n\n";
    currentFile << "Kind: " << code(kind) << "\n\n";
    return type_helper(*this);
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
      if (!s.empty()) {
        string t(o.indents, '#');
        o << t << " " << s << "\n";
      }
    }
    ~section_helper() {
      o.indents--;
    }
  };
  struct type_helper {
    output& o;
    section_helper sh;
    type_helper(output& out) : o(out), sh(o.StartSection("")) {};
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

template<typename T>
void PrintOptionalDescription(output& ss, const T& type)
{
  auto const doc = GetDocString(type);
  if (!doc.empty())
  {
    auto _s = ss.StartSection("Description");
    ss << doc << "\n\n";
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
  constexpr auto Windows_namespace = "Windows.";
  string code = toCode ? "`" : "";
  if (ns.empty()) return type; // basic type
  else if (ns == currentNamespace) {
    return "[" + code + type + code + "](" + type + ")";
  }
  else if (ns._Starts_with(Windows_namespace)) {
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
      /// Missing how to figure out the T in IGeneric<T>
      /// This indicates that we relied on a temporary that got deleted when chaining several calls
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
  PrintOptionalDescription(ss, type);

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
  auto description = GetDocString(prop);
  auto default_val = GetDocDefault(prop);
  if (!default_val.empty()) {
    description += "<br/>default: " + default_val;
  }
  ss << "| " << (isStatic ? (code("static") + "   ") : "") << (readonly ? (code("readonly") + " ") : "") << "| " << name << " | " << type << " | " << description << " | \n";
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
  sstr  << (flags.Static() ? (code("static") + " ") : "")
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

  auto description = GetDocString(method);
  ss << description << "\n\n";
}


void process_field(output& ss, const Field& field) {
  const auto& type = GetType(field.Signature().Type());
  const auto& name = field.Name();
  auto description = GetDocString(field);
  ss << "| " << name << " | " << type << " | " << description << " |\n";
}

void process_struct(output& ss, const TypeDef& type) {
  const auto t = ss.StartType(type.TypeName(), "struct");
  
  PrintOptionalDescription(ss, type);

  const auto fs = ss.StartSection("Fields");

  using entry_t = pair<string_view, const Field>;
  std::list<entry_t> sorted;
  for (auto const& field : type.FieldList()) {
    sorted.push_back(make_pair<string_view, const Field>(field.Name(), Field(field)));
  }
  sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });
  ss << "| Name | Type | Description |" << "\n" << "|---|---|---|" << "\n";
  for (auto const& field : sorted) {
    process_field(ss, field.second);
  }
}

void process_delegate(output& ss, const TypeDef& type) {
  const auto t = ss.StartType(type.TypeName(), "delegate");
  PrintOptionalDescription(ss, type);
  for (auto const& method : type.MethodList()) {
    constexpr auto invokeName = "Invoke";
    const auto& name = method.Name();
    if (method.SpecialName() && name == invokeName) {
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
      if (i == 0) {
        ss << "Implements: ";
      }
      else {
        ss << ", ";
      }
      i++;
      const auto& ifaceName = string(ii.Interface().TypeRef().TypeName());
      ss << ToString(ii.Interface());
      interfaceImplementations[ifaceName].push_back(TypeDef(type));
    }
    ss << "\n\n";
  }

  PrintOptionalDescription(ss, type);

  {
    using entry_t = pair<string_view, const Property>;
    std::list<entry_t> sorted;
    for (auto const& prop: type.PropertyList()) {
      sorted.push_back(make_pair<string_view, const Property>(prop.Name(), Property(prop)));
    }
    sorted.sort([](const entry_t& x, const entry_t& y) { return x.first < y.first; });
    if (!sorted.empty()) {
      auto ps = ss.StartSection("Properties");
      ss << "|   | Name|Type|Description|" << "\n"
         << "|---|-----|----|-----------|" << "\n";
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

string link(string_view n) {
  return "- [" + code(n) + "](" + string(n) + ")";
}

void write_index(string_view namespaceName, const cache::namespace_members& ns) {
  ofstream index("out/index.md");
  index << "# namespace " << namespaceName << "\n";

  index << "## Enums" << "\n";
  for (auto const& enumEntry : ns.enums) {
    index << link(enumEntry.TypeName()) << "\n";
  }

  index << "## Interfaces" << "\n";
  for (auto const& interfaceEntry : ns.interfaces) {
    index << link(interfaceEntry.TypeName()) << "\n";
  }

  index << "## Structs" << "\n";
  for (auto const& structEntry : ns.structs) {
    index << link(structEntry.TypeName()) << "\n";
  }

  index << "## Classes" << "\n";

  for (auto const& classEntry : ns.classes) {
    index << link(classEntry.TypeName()) << "\n";
  }

  index << "## Delegates" << "\n";
  for (auto const& delegateEntry : ns.delegates) {
    index << link(delegateEntry.TypeName()) << "\n";
  }
}

bool shouldSkipInterface(const TypeDef& interfaceEntry) {
#ifdef DEBUG
  auto iname = interfaceEntry.TypeName();
#endif
  return hasAttribute(interfaceEntry.CustomAttribute(), "StaticAttribute") || 
    hasAttribute(interfaceEntry.CustomAttribute(), "ExclusiveToAttribute");
}

void process(output& ss, string_view namespaceName, const cache::namespace_members& ns) {

  for (auto const& enumEntry : ns.enums) {
    process_enum(ss, enumEntry);
  }

  for (auto const& classEntry : ns.classes) {
    process_class(ss, classEntry, "class");
  }

  for (auto const& interfaceEntry : ns.interfaces) {
    if (!shouldSkipInterface(interfaceEntry)) {
      process_class(ss, interfaceEntry, "interface");
    }
  }

  for (auto const& structEntry : ns.structs) {
    process_struct(ss, structEntry);
  }


  for (auto const& delegateEntry : ns.delegates) {
    process_delegate(ss, delegateEntry);
  }

  write_index(namespaceName, ns);
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
    process(o, namespaceEntry.first, namespaceEntry.second);
  }
  return 0;
}
