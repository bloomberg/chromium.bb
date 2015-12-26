// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>

#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"

namespace functions {

namespace {

// On Windows, provide a custom implementation of base::WriteFile. Sometimes
// the base version fails, especially on the bots. The guess is that Windows
// Defender or other antivirus programs still have the file open (after
// checking for the read) when the write happens immediately after. This
// version opens with FILE_SHARE_READ (normally not what you want when
// replacing the entire contents of the file) which lets us continue even if
// another program has the file open for reading. See http://crbug.com/468437
#if defined(OS_WIN)
int DoWriteFile(const base::FilePath& filename, const char* data, int size) {
  base::win::ScopedHandle file(::CreateFile(
      filename.value().c_str(),
      GENERIC_WRITE,
      FILE_SHARE_READ,
      NULL,
      CREATE_ALWAYS,
      0,
      NULL));
  if (!file.IsValid()) {
    PLOG(ERROR) << "CreateFile failed for path "
                  << base::UTF16ToUTF8(filename.value());
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file.Get(), data, size, &written, NULL);
  if (result && static_cast<int>(written) == size)
    return written;

  if (!result) {
    // WriteFile failed.
    PLOG(ERROR) << "writing file " << base::UTF16ToUTF8(filename.value())
                << " failed";
  } else {
    // Didn't write all the bytes.
    LOG(ERROR) << "wrote" << written << " bytes to "
               << base::UTF16ToUTF8(filename.value()) << " expected " << size;
  }
  return -1;
}
#else
int DoWriteFile(const base::FilePath& filename, const char* data, int size) {
  return base::WriteFile(filename, data, size);
}
#endif

}  // namespace

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
    "  One use for write_file is to write a list of inputs to an script\n"
    "  that might be too long for the command line. However, it is\n"
    "  preferrable to use response files for this purpose. See\n"
    "  \"gn help response_file_contents\".\n"
    "\n"
    "  TODO(brettw) we probably need an optional third argument to control\n"
    "  list formatting.\n"
    "\n"
    "Arguments\n"
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
  const SourceDir& cur_dir = scope->GetSourceDir();
  SourceFile source_file = cur_dir.ResolveRelativeFile(args[0], err,
      scope->settings()->build_settings()->root_path_utf8());
  if (err->has_error())
    return Value();
  if (!EnsureStringIsInOutputDir(
          scope->settings()->build_settings()->build_dir(),
          source_file.value(), args[0].origin(), err))
    return Value();
  g_scheduler->AddWrittenFile(source_file);  // Track that we wrote this file.

  // Track how to recreate this file, since we write it a gen time.
  // Note this is a hack since the correct output is not a dependency proper,
  // but an addition of this file to the output of the gn rule that writes it.
  // This dependency will, however, cause the gen step to be re-run and the
  // build restarted if the file is missing.
  g_scheduler->AddGenDependency(
      scope->settings()->build_settings()->GetFullPath(source_file));

  // Compute output.
  std::ostringstream contents;
  if (args[1].type() == Value::LIST) {
    const std::vector<Value>& list = args[1].list_value();
    for (const auto& cur : list)
      contents << cur.ToString(false) << std::endl;
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
  if (DoWriteFile(file_path, new_contents.c_str(), int_size)
      != int_size) {
    *err = Err(function->function(), "Unable to write file.",
               "I was writing \"" + FilePathToUTF8(file_path) + "\".");
    return Value();
  }
  return Value();
}

}  // namespace functions
