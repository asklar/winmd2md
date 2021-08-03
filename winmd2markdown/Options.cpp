#include "Options.h"

#define BOOL_SWITCH_SETTER(x)   0, [](options* o, std::string ) { o->x = true; }
#define STRING_SWITCH_SETTER(x) 1, [](options*o, std::string value) { o->x = value; }

const std::vector<option>  get_option_names() {
  static const std::vector<option> option_names = {
    { "experimental", "Include APIs marked [experimental]", BOOL_SWITCH_SETTER(outputExperimental) },
    { "propsAsTable", "Output Properties as a table", BOOL_SWITCH_SETTER(propertiesAsTable) },
    { "fieldsAsTable", "Output Fields as a table", BOOL_SWITCH_SETTER(fieldsAsTable) },
    { "?", "Display help", BOOL_SWITCH_SETTER(help) },
    { "help", "Display help", BOOL_SWITCH_SETTER(help) },
    { "sdkVersion", "Windows SDK version number to use (e.g. 10.0.18362.0)", STRING_SWITCH_SETTER(sdkVersion) },
    { "apiVersion", "API version number to use (e.g. 0.64). This is the version that the WinMD corresponds to.", STRING_SWITCH_SETTER(apiVersion) },
    { "fileSuffix", "File suffix to append to each generated markdown file. Default is \"-api-windows\"", STRING_SWITCH_SETTER(fileSuffix) },
    { "outputDirectory", "Directory where output will be written. Default is \"out\"", STRING_SWITCH_SETTER(outputDirectory) },
    { "printReferenceGraph", "Displays the list of types that reference each type", BOOL_SWITCH_SETTER(printReferenceGraph )},
    { "strictReferences", "Produce an error when failing to resolve a reference", BOOL_SWITCH_SETTER(strictReferences)},
  };
  return option_names;
}
