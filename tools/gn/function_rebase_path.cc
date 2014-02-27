// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/build_settings.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/value.h"

namespace functions {

namespace {

enum SeparatorConversion {
  SEP_TO_SLASH,  // All slashes to forward.
  SEP_TO_SYSTEM,  // Slashes to system ones.
};

// Does the specified path separator conversion in-place.
void ConvertSlashes(std::string* str, SeparatorConversion mode) {
#if defined(OS_WIN)
  if (mode == SEP_TO_SYSTEM)
    std::replace(str->begin(), str->end(), '/', '\\');
  else
#endif
  if (mode == SEP_TO_SLASH)
    std::replace(str->begin(), str->end(), '\\', '/');
}

// We want the output to match the input in terms of ending in a slash or not.
// Through all the transformations, these can get added or removed in various
// cases.
void MakeSlashEndingMatchInput(const std::string& input, std::string* output) {
  if (EndsWithSlash(input)) {
    if (!EndsWithSlash(*output))  // Preserve same slash type as input.
      output->push_back(input[input.size() - 1]);
  } else {
    if (EndsWithSlash(*output))
      output->resize(output->size() - 1);
  }
}

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

  if (IsSlash(value[value_size - num_dots - 1]))
    return true;  // String is a [back]slash followed by 0 or more dots.

  // Anything else.
  return false;
}

Value ConvertOnePath(const Scope* scope,
                     const FunctionCallNode* function,
                     const Value& value,
                     const SourceDir& from_dir,
                     const SourceDir& to_dir,
                     bool convert_to_system_absolute,
                     SeparatorConversion separator_conversion,
                     Err* err) {
  Value result;  // Ensure return value optimization.

  if (!value.VerifyTypeIs(Value::STRING, err))
    return result;
  const std::string& string_value = value.string_value();

  bool looks_like_dir = ValueLooksLikeDir(string_value);

  // System-absolute output special case.
  if (convert_to_system_absolute) {
    base::FilePath system_path;
    if (looks_like_dir) {
      system_path = scope->settings()->build_settings()->GetFullPath(
          from_dir.ResolveRelativeDir(string_value));
    } else {
      system_path = scope->settings()->build_settings()->GetFullPath(
          from_dir.ResolveRelativeFile(string_value));
    }
    result = Value(function, FilePathToUTF8(system_path));
    if (looks_like_dir)
      MakeSlashEndingMatchInput(string_value, &result.string_value());
    ConvertPathToSystem(&result.string_value());
    return result;
  }

  if (from_dir.is_system_absolute() || to_dir.is_system_absolute()) {
    *err = Err(function, "System-absolute directories are not supported for "
        "the source or dest dir for rebase_path. It would be nice to add this "
        "if you're so inclined!");
    return result;
  }

  result = Value(function, Value::STRING);
  if (looks_like_dir) {
    result.string_value() = RebaseSourceAbsolutePath(
        from_dir.ResolveRelativeDir(string_value).value(),
        to_dir);
    MakeSlashEndingMatchInput(string_value, &result.string_value());
  } else {
    result.string_value() = RebaseSourceAbsolutePath(
        from_dir.ResolveRelativeFile(string_value).value(),
        to_dir);
  }

  ConvertSlashes(&result.string_value(), separator_conversion);
  return result;
}

}  // namespace

const char kRebasePath[] = "rebase_path";
const char kRebasePath_Help[] =
    "rebase_path: Rebase a file or directory to another location.\n"
    "\n"
    "  converted = rebase_path(input,\n"
    "                          new_base = \"\",\n"
    "                          current_base = \".\",\n"
    "                          path_separators = \"to_slash\")\n"
    "\n"
    "  Takes a string argument representing a file name, or a list of such\n"
    "  strings and converts it/them to be relative to a different base\n"
    "  directory.\n"
    "\n"
    "  When invoking the compiler or scripts, GN will automatically convert\n"
    "  sources and include directories to be relative to the build directory.\n"
    "  However, if you're passing files directly in the \"args\" array or\n"
    "  doing other manual manipulations where GN doesn't know something is\n"
    "  a file name, you will need to convert paths to be relative to what\n"
    "  your tool is expecting.\n"
    "\n"
    "  The common case is to use this to convert paths relative to the\n"
    "  current directory to be relative to the build directory (which will\n"
    "  be the current directory when executing scripts).\n"
    "\n"
    "Arguments\n"
    "\n"
    "  input\n"
    "      A string or list of strings representing file or directory names\n"
    "      These can be relative paths (\"foo/bar.txt\"), system absolute\n"
    "      paths (\"/foo/bar.txt\"), or source absolute paths\n"
    "      (\"//foo/bar.txt\").\n"
    "\n"
    "  new_base\n"
    "      The directory to convert the paths to be relative to. This can be\n"
    "      an absolute path or a relative path (which will be treated\n"
    "      as being relative to the current BUILD-file's directory).\n"
    "\n"
    "      As a special case, if new_base is the empty string (the default),\n"
    "      all paths will be converted to system-absolute native style paths\n"
    "      with system path separators. This is useful for invoking external\n"
    "      programs.\n"
    "\n"
    "  current_base\n"
    "      Directory representing the base for relative paths in the input.\n"
    "      If this is not an absolute path, it will be treated as being\n"
    "      relative to the current build file. Use \".\" (the default) to\n"
    "      convert paths from the current BUILD-file's directory.\n"
    "\n"
    "  path_separators\n"
    "      On Windows systems, indicates whether and how path separators\n"
    "      should be converted as part of the transformation. It can be one\n"
    "      of the following strings:\n"
    "       - \"to_slash\" Normalize all types of slashes to forward slashes.\n"
    "         This is the default if this argument is unspecified.\n"
    "       - \"to_system\" Convert to the system path separators\n"
    "         (backslashes on Windows).\n"
    "\n"
    "      On Posix systems there are no path separator transformations\n"
    "      applied. If the new_base is empty (specifying absolute output)\n"
    "      this parameter should not be supplied since paths will always be\n"
    "      converted,\n"
    "\n"
    "Return value\n"
    "\n"
    "  The return value will be the same type as the input value (either a\n"
    "  string or a list of strings). All relative and source-absolute file\n"
    "  names will be converted to be relative to the requested output\n"
    "  System-absolute paths will be unchanged.\n"
    "\n"
    "Example\n"
    "\n"
    "  # Convert a file in the current directory to be relative to the build\n"
    "  # directory (the current dir when executing compilers and scripts).\n"
    "  foo = rebase_path(\"myfile.txt\", root_build_dir)\n"
    "  # might produce \"../../project/myfile.txt\".\n"
    "\n"
    "  # Convert a file to be system absolute:\n"
    "  foo = rebase_path(\"myfile.txt\")\n"
    "  # Might produce \"D:\\source\\project\\myfile.txt\" on Windows or\n"
    "  # \"/home/you/source/project/myfile.txt\" on Linux.\n"
    "\n"
    "  # Convert a file's path separators from forward slashes to system\n"
    "  # slashes.\n"
    "  foo = rebase_path(\"source/myfile.txt\", \".\", \".\", \"to_system\")\n"
    "\n"
    "  # Typical usage for converting to the build directory for a script.\n"
    "  custom(\"myscript\") {\n"
    "    # Don't convert sources, GN will automatically convert these to be\n"
    "    # relative to the build directory when it contructs the command\n"
    "    # line for your script.\n"
    "    sources = [ \"foo.txt\", \"bar.txt\" ]\n"
    "\n"
    "    # Extra file args passed manually need to be explicitly converted\n"
    "    # to be relative to the build directory:\n"
    "    args = [\n"
    "      \"--data\",\n"
    "      rebase_path(\"//mything/data/input.dat\", root_build_dir),\n"
    "      \"--rel\",\n"
    "      rebase_path(\"relative_path.txt\", root_build_dir)\n"
    "    ]\n"
    "  }\n";

Value RunRebasePath(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err) {
  Value result;

  // Argument indices.
  static const size_t kArgIndexInputs = 0;
  static const size_t kArgIndexDest = 1;
  static const size_t kArgIndexFrom = 2;
  static const size_t kArgIndexPathConversion = 3;

  // Inputs.
  if (args.size() < 1 || args.size() > 4) {
    *err = Err(function->function(), "Wrong # of arguments for rebase_path.");
    return result;
  }
  const Value& inputs = args[kArgIndexInputs];

  // To path.
  bool convert_to_system_absolute = true;
  SourceDir to_dir;
  const SourceDir& current_dir = scope->GetSourceDir();
  if (args.size() > kArgIndexDest) {
    if (!args[kArgIndexDest].VerifyTypeIs(Value::STRING, err))
      return result;
    if (!args[kArgIndexDest].string_value().empty()) {
      to_dir =
          current_dir.ResolveRelativeDir(args[kArgIndexDest].string_value());
      convert_to_system_absolute = false;
    }
  }

  // From path.
  SourceDir from_dir;
  if (args.size() > kArgIndexFrom) {
    if (!args[kArgIndexFrom].VerifyTypeIs(Value::STRING, err))
      return result;
    from_dir =
        current_dir.ResolveRelativeDir(args[kArgIndexFrom].string_value());
  } else {
    // Default to current directory if unspecified.
    from_dir = current_dir;
  }

  // Path conversion.
  SeparatorConversion sep_conversion = SEP_TO_SLASH;
  if (args.size() > kArgIndexPathConversion) {
    if (convert_to_system_absolute) {
      *err = Err(function, "Can't specify slash conversion.",
          "You specified absolute system path output by using an empty string "
          "for the destination directory on rebase_path(). In this case, you "
          "can't specify slash conversion.");
      return result;
    }

    if (!args[kArgIndexPathConversion].VerifyTypeIs(Value::STRING, err))
      return result;
    const std::string& sep_string =
        args[kArgIndexPathConversion].string_value();
    if (sep_string == "to_slash") {
      sep_conversion = SEP_TO_SLASH;
    } else if (sep_string == "to_system") {
      sep_conversion = SEP_TO_SYSTEM;
    } else {
      *err = Err(args[kArgIndexPathConversion],
          "Invalid path separator conversion mode.",
          "I was expecting \"to_slash\" or \"to_system\" and\n"
          "you gave me \"" + args[kArgIndexPathConversion].string_value() +
          "\".");
      return result;
    }
  }

  if (inputs.type() == Value::STRING) {
    return ConvertOnePath(scope, function, inputs,
                          from_dir, to_dir, convert_to_system_absolute,
                          sep_conversion, err);

  } else if (inputs.type() == Value::LIST) {
    result = Value(function, Value::LIST);
    result.list_value().reserve(inputs.list_value().size());

    for (size_t i = 0; i < inputs.list_value().size(); i++) {
      result.list_value().push_back(
          ConvertOnePath(scope, function, inputs.list_value()[i],
                         from_dir, to_dir, convert_to_system_absolute,
                         sep_conversion, err));
      if (err->has_error()) {
        result = Value();
        return result;
      }
    }
    return result;
  }

  *err = Err(function->function(),
             "rebase_path requires a list or a string.");
  return result;
}

}  // namespace functions
