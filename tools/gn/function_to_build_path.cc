// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/build_settings.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/path_output.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/value.h"

namespace functions {

namespace {

// Returns true if the given value looks like a directory, otherwise we'll
// assume it's a file.
bool ValueLooksLikeDir(const std::string& value) {
  if (value.empty())
    return true;
  size_t value_size = value.size();

  // Count the number of dots at the end of the string.
  size_t num_dots = 0;
  while (num_dots < value_size && value[value_size - num_dots - 1] == '.')
    num_dots++;

  if (num_dots == value.size())
    return true;  // String is all dots.

  if (value[value_size - num_dots - 1] == '/' ||
      value[value_size - num_dots - 1] == '\\')
    return true;  // String is a [back]slash followed by 0 or more dots.

  // Anything else.
  return false;
}

Value ConvertOneBuildPath(const Scope* scope,
                          const FunctionCallNode* function,
                          const Value& value,
                          const PathOutput& path_output,
                          Err* err) {
  if (!value.VerifyTypeIs(Value::STRING, err))
    return Value();

  const std::string& string_value = value.string_value();

  std::ostringstream buffer;
  if (ValueLooksLikeDir(string_value)) {
    SourceDir absolute =
        scope->GetSourceDir().ResolveRelativeDir(string_value);
    path_output.WriteDir(buffer, absolute, PathOutput::DIR_NO_LAST_SLASH);
  } else {
    SourceFile absolute =
        scope->GetSourceDir().ResolveRelativeFile(string_value);
    path_output.WriteFile(buffer, absolute);
  }
  return Value(function, buffer.str());
}

}  // namespace

const char kToBuildPath[] = "to_build_path";
const char kToBuildPath_Help[] =
    "to_build_path: Rebase a file or directory to the build output dir.\n"
    "\n"
    "  <converted> = to_build_path(<file_or_path_string_or_list>)\n"
    "\n"
    "  Takes a string argument representing a file name, or a list of such\n"
    "  strings and converts it/them to be relative to the root build output\n"
    "  directory (which is the current directory when running scripts).\n"
    "\n"
    "  The input can be:\n"
    "   - Paths relative to the BUILD file like \"foo.txt\".\n"
    "   - Source-root absolute paths like \"//foo/bar/foo.txt\".\n"
    "   - System absolute paths like \"/usr/include/foo.h\" or\n"
    "     \"/C:/foo/bar.h\" (these will be passed unchanged).\n"
    "   - A list of such values (the result will be a list of each item\n"
    "     converted as per the above description).\n"
    "\n"
    "  Normally for sources and in cases where GN is providing file names\n"
    "  to a tool, the paths will automatically be converted to be relative\n"
    "  to the build directory. However, if you pass additional arguments,\n"
    "  GN won't know that the string is actually a file path. These will\n"
    "  need to be manually converted to be relative to the build dir using\n"
    "  to_build_path().\n"
    "\n"
    "  Trailing slashes will not be reflected in the output.\n"
    "\n"
    "  Additionally, on Windows, slashes will be converted to backslashes.\n"
    "\n"
    "Example:\n"
    "  custom(\"myscript\") {\n"
    "    # Don't use for sources, GN will automatically convert these since\n"
    "    # it knows they're files.\n"
    "    sources = [ \"foo.txt\", \"bar.txt\" ]\n"
    "\n"
    "    # Extra file args passed manually need to be explicitly converted:\n"
    "    args = [ \"--data\", to_build_path(\"//mything/data/input.dat\"),\n"
    "             \"--rel\", to_build_path(\"relative_path.txt\") ]\n"
    "  }\n";

Value RunToBuildPath(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "to_build_path takes one argument.");
    return Value();
  }

  const Value& value = args[0];
  PathOutput path_output(scope->settings()->build_settings()->build_dir(),
                         ESCAPE_NONE, true);

  if (value.type() == Value::STRING) {
    return ConvertOneBuildPath(scope, function, value, path_output, err);

  } else if (value.type() == Value::LIST) {
    Value ret(function, Value::LIST);
    ret.list_value().reserve(value.list_value().size());

    for (size_t i = 0; i < value.list_value().size(); i++) {
      ret.list_value().push_back(
          ConvertOneBuildPath(scope, function, value.list_value()[i],
                              path_output, err));
    }
    return ret;
  }

  *err = Err(function->function(),
             "to_build_path requires a list or a string.");
  return Value();
}

}  // namespace functions
