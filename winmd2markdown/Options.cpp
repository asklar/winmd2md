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
    { "sdkVersion", "SDK version number to use (e.g. 10.0.18362.0)", STRING_SWITCH_SETTER(sdkVersion) },
    { "fileSuffix", "File suffix to append to each generated markdown file. Default is \"-api-windows\"", STRING_SWITCH_SETTER(fileSuffix) },
  };
  return option_names;
}

options* g_opts = nullptr;