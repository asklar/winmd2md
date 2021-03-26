#include <string>
#include <sstream>
#include <algorithm>
#include <winmd_reader.h>

#include "Format.h"
#include "Program.h"

using namespace std;
using namespace winmd::reader;

string Formatter::MakeMarkdownReference(const string& ns, const string& type, const string& propertyName) {
  string anchor = type;
  string link = type;
  if (ns != program->currentNamespace && !ns.empty()) {
    return typeToMarkdown(ns, type + (!propertyName.empty() ? ("." + propertyName) : ""), true);
  }
  if (!propertyName.empty()) {
    anchor += (!type.empty() ? "." : "") + propertyName;
    auto p = propertyName;
    if (p == "Properties") {
      // special case a property or method that might be named Properties, in this case we usually want to it and not the Properties section
      p = "Properties-1";
    }
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    link += "#" + p;
  }
  else {
    // this is a type reference
    auto dot = type.rfind('.');
    if (dot != -1) {
      auto ns = type.substr(0, dot);
      auto typeName = type.substr(dot + 1);
      return typeToMarkdown(ns, typeName, true);
    }
  }
  return "[" + code(anchor) + "](" + link + ")";
}

string Formatter::MakeXmlReference(const string& ns, const string& type, const string& propertyName) {
  return R"(<see cref=")" + (ns.empty() ? program->currentNamespace : ns) + "." + type + ((!type.empty() && !propertyName.empty()) ? "." : "") + propertyName + R"("/>)";
}

std::string code(std::string_view v) {
  return "`" + std::string(v) + "`";
}


string link(string_view n) {
  //if (IsBuiltInType(n)) return string(n);
  return "- [" + code(n) + "](" + string(n) + ")";
}

bool isIdentifierChar(char x) {
  return isalnum(x) || x == '_' || x == '.';
}


string Formatter::ResolveReferences(string sane, Converter converter) {
  stringstream ss;

  for (size_t input = 0; input < sane.length(); input++) {
    if (sane[input] == '@') {
      auto firstNonIdentifier = std::find_if(sane.begin() + input + 1, sane.end(), [](auto& x) { return !isIdentifierChar(x); });
      if (firstNonIdentifier != sane.begin() && *(firstNonIdentifier - 1) == '.') {
        firstNonIdentifier--;
      }
      auto next = firstNonIdentifier - sane.begin() - 1;
      auto reference = sane.substr(input + 1, next - input);
      // The reference could either be a @TypeName.Property
      // Or it could be a @.Property
      // Or it could be a @TypeName
      // So we have to disambiguate whether we are dealing with a type or a property
      const auto dot = reference.rfind('.');
      const string prefix = reference.substr(0, dot);
      const string suffix = (dot != -1) ? reference.substr(dot + 1) : "";
      TypeDef referredType;
      if (referredType = program->cache->find(prefix, suffix)) {
        ss << (this->*converter)(prefix, suffix, "");
      }
      else if (suffix.empty() && (referredType = program->cache->find(program->currentNamespace, prefix))) {
        ss << (this->*converter)(program->currentNamespace, prefix, ""); // typeToMarkdown(referredType.TypeNamespace(), string(referredType.TypeName()), true);
      }
      else {
        if (referredType = program->cache->find(program->currentNamespace, prefix)) {
          // reference is @LocalType.Property
          ss << (this->*converter)("", prefix, suffix);
        }
        else {
          const auto dot2 = prefix.rfind('.');
          if (dot2 != -1 || prefix.empty()) { // either it's a Ns.Type.Prop, or its a .Prop for the current type
            const string ns = prefix.substr(0, dot2);
            const string typeName = prefix.substr(dot2 + 1);

            ss << (this->*converter)(ns, typeName, suffix);
          }
          else {
            throw exception(("unknown reference: " + reference).c_str());
          }
        }
      }
      input = next;
      continue;
    }
    ss << sane[input];
  }


  return ss.str();
}

std::string Formatter::typeToMarkdown(std::string_view ns, std::string type, bool toCode, string urlSuffix)
{
  constexpr std::string_view docs_msft_com_namespaces[] = {
    "Windows.",
    "Microsoft.",
  };
  string code = toCode ? "`" : "";
  if (ns.empty()) return type; // basic type
  else if (ns == program->currentNamespace) {
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

string Formatter::GetNamespacePrefix(std::string_view ns)
{
  if (ns == program->currentNamespace) return "";
  else return string(ns) + ".";
}

std::string GetType(const winmd::reader::TypeSig& type);

string Formatter::ToString(const coded_index<TypeDefOrRef>& tdr, bool toCode) {
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

string Formatter::GetType(const TypeSig::value_type& valueType)
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

string Formatter::GetType(const TypeSig& type) {
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

std::string_view Formatter::ToString(ElementType elementType) {
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
    return Program::ObjectClassName;
  default:
    //cout << std::hex << (int)elementType << endl;
    return "{type}";

  }
}
