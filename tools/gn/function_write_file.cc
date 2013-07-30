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

/*
write_file: Read a file into a variable.

  write_file(filename, data)

  If data is a list, the list will be written one-item-per-line with no
  quoting or brackets.

  TODO(brettw) we probably need an optional third argument to control list
  formatting.

Arguments:

  filename:
      Filename to write. This must be within the output directory.

  data:
      The list or string to write.
*/
Value ExecuteWriteFile(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       Err* err) {
  if (args.size() != 2) {
    *err = Err(function->function(), "Wrong number of args to write_file",
               "I expected two arguments.");
    return Value();
  }

  // Compute the file name and make sure it's in the output dir.
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();
  const SourceDir& cur_dir = SourceDirForFunctionCall(function);
  SourceFile source_file = cur_dir.ResolveRelativeFile(args[0].string_value());
  if (!EnsureStringIsInOutputDir(
          scope->settings()->build_settings()->build_dir(),
          source_file.value(), args[0], err))
    return Value();

  // Compute output.
  std::ostringstream contents;
  if (args[1].type() == Value::LIST) {
    const std::vector<Value>& list = args[1].list_value();
    for (size_t i = 0; i < list.size(); i++)
      contents << list[i].ToString() << std::endl;
  } else {
    contents << args[1].ToString();
  }

  // Write file, creating the directory if necessary.
  base::FilePath file_path =
      scope->settings()->build_settings()->GetFullPath(source_file);
  const std::string& contents_string = contents.str();
  if (!file_util::CreateDirectory(file_path.DirName())) {
    *err = Err(function->function(), "Unable to create directory.",
               "I was using \"" + FilePathToUTF8(file_path.DirName()) + "\".");
    return Value();
  }
  if (file_util::WriteFile(file_path,
                           contents_string.c_str(), contents_string.size())
      != static_cast<int>(contents_string.size())) {
    *err = Err(function->function(), "Unable to write file.",
               "I was writing \"" + FilePathToUTF8(file_path) + "\".");
    return Value();
  }
  return Value();
}
