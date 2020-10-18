# winmd2md
Generate markdown documentation stubs from Windows Metadata

## Description
[Windows Metadata](https://docs.microsoft.com/uwp/winrt-cref/winmd-files) is a format to describe types (interfaces, enums, classes, etc.) and APIs for the Windows Runtime.
winmd2md parses a .winmd file you provide it, and outputs one markdown file for every type, plus an index.md that lists the types described. 
Every type will then contain the APIs it contains, and cross-link to other types in the winmd or to docs.microsoft.com if it is a Windows type.
The output is saved to the `out` folder under the current folder.

**Syntax**:  `winmd2md.exe myWinmd.winmd`
