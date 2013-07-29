// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/location.h"

namespace {

std::vector<std::string> GetArgs(const CommandLine& cmdline) {
  CommandLine::StringVector in_args = cmdline.GetArgs();
#if defined(OS_WIN)
  std::vector<std::string> out_args;
  for (size_t i = 0; i < in_args.size(); i++)
    out_args.push_back(base::WideToUTF8(in_args[i]));
  return out_args;
#else
  return in_args;
#endif
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  std::vector<std::string> args = GetArgs(*CommandLine::ForCurrentProcess());

  std::string command;
  if (args.empty()) {
    command = "gen";
  } else {
    command = args[0];
    args.erase(args.begin());
  }

  int retval = 0;
  if (command == "gen") {
    retval = RunGenCommand(args);
  } else if (command == "desc" || command == "wtf") {
    retval = RunDescCommand(args);
  } else if (command == "deps") {
    retval = RunDepsCommand(args);
  } else if (command == "tree") {
    retval = RunTreeCommand(args);
  } else {
    Err(Location(),
        "Command \"" + command + "\" unknown.").PrintToStdout();
    retval = 1;
  }

  exit(retval);  // Don't free memory, it can be really slow!
  return retval;
}
