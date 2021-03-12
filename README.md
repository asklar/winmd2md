# winmd2md
Generate markdown documentation from Windows Metadata

## Description
[Windows Metadata](https://docs.microsoft.com/uwp/winrt-cref/winmd-files) is a format to describe types (interfaces, enums, classes, etc.) and APIs for the Windows Runtime.
winmd2md parses a .winmd file you provide it, and outputs one markdown file for every type, plus an index.md that lists the types described. 
Every type will then contain the APIs it contains, and cross-link to other types in the winmd or to docs.microsoft.com if it is a Windows type.
The output is saved to the `out` folder under the current folder.

**Syntax**:  `winmd2md.exe [options] myWinmd.winmd`

   /experimental          Include APIs marked [experimental]
   /propsAsTable          Output Properties as a table
   /fieldsAsTable         Output Fields as a table
   /? or /help            Display help
   /sdkVersion            SDK version number to use (e.g. 10.0.18362.0)
   /fileSuffix            File suffix to append to each generated markdown file. Default is "-api-windows"
   /outputDirectory       Directory where output will be written. Default is "out"
   /printReferenceGraph   Displays the list of types that reference each type

WinMD2MD will also understand certain custom attributes that you can apply to types and APIs, and use those custom attributes' values:

```csharp
namespace MyAppOrLibrary {
  [attributeusage(target_runtimeclass, target_interface, target_struct, target_enum, target_delegate, target_field, target_property, target_method, target_event)]
  [attributename("doc_string")]
  attribute DocStringAttribute {
    String Content;
  }

  [attributeusage(target_property, target_method)]
  [attributename("doc_default")] attribute DocDefaultAttribute {
    String Content;
  }
  
  // apply it to types and APIs:
  [doc_string("This is the documentation string for this interface")]
  [webhosthidden]
  interface IMyInterface {
    [doc_string("This function does something. See @MyClass.function2 for more info")]
    void f();
  }
  
  [doc_string("This class implements @MyInterface")]
  runtimeclass MyClass : IMyInterface {
    void f();
    [doc_string("This is another function. It is related to @IMyInterface.f.");
    void function2();
    
    [doc_string("This is a sample property")]
    [doc_default("8080")]
    Int32 HttpPort { get; set; }  
  }
}
```
