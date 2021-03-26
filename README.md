# winmd2md
Generate markdown documentation from Windows Metadata

## Description
[Windows Metadata](https://docs.microsoft.com/uwp/winrt-cref/winmd-files) is a format to describe types (interfaces, enums, classes, etc.) and APIs for the Windows Runtime.
winmd2md parses a .winmd file you provide it, and outputs one markdown file for every type, plus an index.md that lists the types described. 
Every type will then contain the APIs it contains, and cross-link to other types in the winmd or to docs.microsoft.com if it is a Windows type.
The output is saved to the `out` folder under the current folder.

**Syntax**:  `winmd2md.exe [options] myWinmd.winmd`
```
   /experimental          Include APIs marked [experimental]
   /propsAsTable          Output Properties as a table
   /fieldsAsTable         Output Fields as a table
   /? or /help            Display help
   /sdkVersion            SDK version number to use (e.g. 10.0.18362.0)
   /fileSuffix            File suffix to append to each generated markdown file. Default is "-api-windows"
   /outputDirectory       Directory where output will be written. Default is "out"
   /printReferenceGraph   Displays the list of types that reference each type
```

WinMD2MD will also understand certain custom attributes that you can apply to types and APIs, and use those custom attributes' values:

```csharp
namespace MyAppOrLibrary {

  /// Add doc_string to your app's IDL
  [attributeusage(target_runtimeclass, target_interface, target_struct, target_enum, target_delegate, target_field, target_property, target_method, target_event)]
  [attributename("doc_string")]
  attribute DocStringAttribute {
    String Content;
  }

  /// Add doc_default to your app's IDL
  [attributeusage(target_property, target_method)]
  [attributename("doc_default")] attribute DocDefaultAttribute {
    String Content;
  }
  
  // apply it to types and APIs:
  [doc_string("This is the documentation string for this interface")]
  [webhosthidden]
  interface IMyInterface {
    [doc_string("This function does something. See @MyClass.Function2 for more info")]
    void Function1();
  }
  
  [doc_string("This class implements @MyInterface")]
  runtimeclass MyClass : IMyInterface {
    void Function1();
    [doc_string("This is another function. It is related to @IMyInterface.Function1.");
    void Function2();
    
    [doc_string("This is a sample property")]
    [doc_default("8080")]
    Int32 HttpPort { get; set; }  
    
    [deprecated("Don't use this property, use @.HttpPort instead", "deprecate", 42)]
    Int32 DeprecatedHttpPort { get; set; }
    
    [experimental]
    void ExperimentalFunction();
  }
  
  
}
```

### Features
- WinMD2MD will pull out the information from the doc_string, doc_default, experimental and deprecated attributes,
- Helps document type/API information
- Reasons over references between the different types in your assembly and add a back-links section at the bottom of the page.
- You can also include markdown in your strings.
- You can link to other types and their members with the `@Type.Member`, or `@Type` syntax. This produces hyperlinks to types either in your own assembly, or on docs.microsoft.com if the type is in the Windows or Microsoft namespace.
- Finally, WinMD2MD will also write an index page with links to all the types in the assembly.

### See it in action
If you want to see what the generated markdown looks like you can check out the React Native for Windows repo/website:
- Markdown docs where we have text content (doc_string / doc_default attributes): [react-native-windows-samples/docs](https://github.com/microsoft/react-native-windows-samples/tree/master/docs)
  - Website link: [namespace Microsoft.ReactNative · React Native for Windows + macOS](https://microsoft.github.io/react-native-windows/docs/next/Native-API-Reference)
- Markdown docs with just the type information (no doc_string/doc_default):  [react-native-windows-samples/website/versioned_docs/version-0.63](https://github.com/microsoft/react-native-windows-samples/tree/master/website/versioned_docs/version-0.63)
  - Website link: [namespace Microsoft.ReactNative · React Native for Windows + macOS](https://microsoft.github.io/react-native-windows/docs/Native-API-Reference)

