// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "base/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"

namespace functions {

const char kWriteFile[] = "write_file";
const char kWriteFile_HelpShort[] =
    "write_file: Write a file to disk.";
const char kWriteFile_Help[] =
    "write_file: Write a file to disk.\n"
    "\n"
    "  write_file(filename, data)\n"
    "\n"
    "  If data is a list, the list will be written one-item-per-line with no\n"
    "  quoting or brackets.\n"
    "\n"
    "  If the file exists and the contents are identical to that being\n"
    "  written, the file will not be updated. This will prevent unnecessary\n"
    "  rebuilds of targets that depend on this file.\n"
    "\n"
    "  TODO(brettw) we probably need an optional third argument to control\n"
    "  list formatting.\n"
    "\n"
    "Arguments:\n"
    "\n"
    "  filename\n"
    "      Filename to write. This must be within the output directory.\n"
    "\n"
    "  data:\n"
    "      The list or string to write.\n";

Value RunWriteFile(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   Err* err) {
  if (args.size() != 2) {
    *err = Err(function->function(), "Wrong number of arguments to write_file",
               "I expected two arguments.");
    return Value();
  }

  // Compute the file name and make sure it's in the output dir.
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();
  const SourceDir& cur_dir = scope->GetSourceDir();
  SourceFile source_file = cur_dir.ResolveRelativeFile(args[0].string_value());
  if (!EnsureStringIsInOutputDir(
          scope->settings()->build_settings()->build_dir(),
          source_file.value(), args[0].origin(), err))
    return Value();

  // Compute output.
  std::ostringstream contents;
  if (args[1].type() == Value::LIST) {
    const std::vector<Value>& list = args[1].list_value();
    for (size_t i = 0; i < list.size(); i++)
      contents << list[i].ToString(false) << std::endl;
  } else {
    contents << args[1].ToString(false);
  }
  const std::string& new_contents = contents.str();
  base::FilePath file_path =
      scope->settings()->build_settings()->GetFullPath(source_file);

  // Make sure we're not replacing the same contents.
  std::string existing_contents;
  if (base::ReadFileToString(file_path, &existing_contents) &&
      existing_contents == new_contents)
    return Value();  // Nothing to do.

  // Write file, creating the directory if necessary.
  if (!base::CreateDirectory(file_path.DirName())) {
    *err = Err(function->function(), "Unable to create directory.",
               "I was using \"" + FilePathToUTF8(file_path.DirName()) + "\".");
    return Value();
  }

  int int_size = static_cast<int>(new_contents.size());
  if (base::WriteFile(file_path, new_contents.c_str(), int_size)
      != int_size) {
    *err = Err(function->function(), "Unable to write file.",
               "I was writing \"" + FilePathToUTF8(file_path) + "\".");
    return Value();
  }
  return Value();
}

}  // namespace functions
