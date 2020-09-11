// winmd2markdown.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <string>
#include <filesystem>
#include <winmd_reader.h>
using namespace std;
using namespace winmd::reader;

std::string ToString(ElementType elementType) {
  switch (elementType) {
  case ElementType::Boolean:
    return "bool";
  case ElementType::I2:
    return "short";
  case ElementType::I4:
    return "int";
  case ElementType::I8:
    return "long long";
  case ElementType::U2:
    return "unsigned short";
  case ElementType::U4:
    return "unsigned int";
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
    return "{Object}";
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

void print_enum(const TypeDef& type) {
  cout << "enum " << type.TypeName() << endl;
  for (auto const& value : type.FieldList()) {
    if (value.Flags().SpecialName()) {
      continue;
    }
    cout << "    " << value.Name() << " = " << std::hex << "0x" << ((value.Signature().Type().element_type() == ElementType::U4) ? value.Constant().ValueUInt32() : value.Constant().ValueInt32()) << endl;
  }
}

void print_property(const Property& prop) {
  cout << "    " << prop.Name() << endl;
}

void print_method(const MethodDef& method) {
  std::string returnType;
  if (method.Signature().ReturnType()) {
    auto elementType = method.Signature().ReturnType().Type().element_type();
    returnType = ToString(elementType);
    //m.Signature().ReturnType().Type().element_type() == winmd::reader::ElementType::
  }
  else {
    returnType = "void";
  }
  //auto index = m.Signature().ReturnType().Type().Type().index();
  //winmd::reader::coded_index<winmd::reader::TypeDefOrRef> retType(&db.get_table<winmd::reader::TypeDefOrRef>(), (uint32_t)index);
  const auto flags = method.Flags();
  std::cout << "    " << (flags.Static() ? "static " : "") << returnType << " " << method.Name() << "(...)" << endl;
}

string GetType(const TypeSig& type) {
  if (type.element_type() != ElementType::Class) {
    return ToString(type.element_type());
  }
  else {
    return "some type";
  }
}

void print_field(const Field& field) {
  cout << "    " << GetType(field.Signature().Type())  << " " << field.Name() << endl;
}

void print_struct(const TypeDef& type) {
  cout << "struct " << type.TypeName() << endl;
  for (auto const& field : type.FieldList()) {
    print_field(field);
  }
}

void print_class(const TypeDef& type) {
  cout << "class " << type.TypeName() << endl;
  for (auto const& prop : type.PropertyList()) {
    print_property(prop);
  }
  for (auto const& method : type.MethodList()) {
    if (method.SpecialName()) {
      continue; // get_ / put_ methods that are properties
    }
    print_method(method);
  }
}

void print_type(const TypeDef& type) {
  cout << "type: " << type.TypeName() << endl;
}

void print(const cache::namespace_members& ns) {
  for (auto const& enumEntry : ns.enums) {
    print_enum(enumEntry);
  }

  for (auto const& structEntry : ns.structs) {
    print_struct(structEntry);
  }

  for (auto const& classEntry : ns.classes) {
    print_class(classEntry);
  }
}

int main()
{
  std::string winmdPath(R"(E:\rnw\vnext\build\x86\Debug\Microsoft.ReactNative\Merged\Microsoft.ReactNative.winmd)");

  winmd::reader::database db(winmdPath);
  cache cache(winmdPath);
  
  for (auto const& namespaceEntry : cache.namespaces()) {
    cout << "namespace " << namespaceEntry.first << endl;
    print(namespaceEntry.second);
  }
}


//  db.get_cache().namespaces()
//  for (auto const& type : db.TypeDef) {
//    if (!type.Flags().WindowsRuntime()) {
//      continue;
//    }
//    const auto name = type.TypeName();
//    const bool abstract = type.Flags().Abstract();
//
//    std::cout << GetTypeKind(type) << " " << type.TypeName() << std::endl;
//    if (!type.Flags().Abstract() && type.is_enum()) {
//      for (auto const& value : type.FieldList()) {
//        if (value.Flags().SpecialName()) {
//          continue;
//        }
//        cout << "    " << value.Name() << " = " << std::hex << "0x" << ((value.Signature().Type().element_type() == ElementType::U4) ? value.Constant().ValueUInt32() : value.Constant().ValueInt32()) << endl;
//      }
//    }
//    for (auto const& p : type.PropertyList()) {
//      cout << "    " << p.Name() << endl;
//    }
//
//    for (auto const& m : type.MethodList()) {
//      //for (auto const& p : m.Signature().Params()) {
//      ////  p.Type().Type()._Storage().
//      //}
//      if (m.SpecialName()) continue; // get_ / put_ methods that are properties
//      
//      std::string returnType;
//      if (m.Signature().ReturnType()) {
//        auto elementType = m.Signature().ReturnType().Type().element_type();
//        returnType = ToString(elementType);
//        //m.Signature().ReturnType().Type().element_type() == winmd::reader::ElementType::
//      }
//      else {
//        returnType = "void";
//      }
//      //auto index = m.Signature().ReturnType().Type().Type().index();
//      //winmd::reader::coded_index<winmd::reader::TypeDefOrRef> retType(&db.get_table<winmd::reader::TypeDefOrRef>(), (uint32_t)index);
//      const auto flags = m.Flags();
//      std::cout << "    " << (flags.Static() ? "static " : "") << returnType << " " << m.Name() << "(...)" << endl;
//    }
//  }
//}
