// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/delegate_execute_util.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"

namespace delegate_execute {

base::CommandLine CommandLineFromParameters(const wchar_t* params) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);

  if (params) {
    base::string16 command_string(L"noprogram.exe ");
    command_string.append(params);
    command_line.ParseFromString(command_string);
    command_line.SetProgram(base::FilePath());
  }

  return command_line;
}

base::CommandLine MakeChromeCommandLine(const base::FilePath& chrome_exe,
                                        const base::CommandLine& params,
                                        const base::string16& argument) {
  base::CommandLine chrome_cmd(params);
  chrome_cmd.SetProgram(chrome_exe);

  if (!argument.empty())
    chrome_cmd.AppendArgNative(argument);

  return chrome_cmd;
}

base::string16 ParametersFromSwitch(const char* a_switch) {
  if (!a_switch)
    return base::string16();

  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);

  cmd_line.AppendSwitch(a_switch);

  base::string16 command_string(cmd_line.GetCommandLineString());
  base::TrimWhitespace(command_string, base::TRIM_ALL, &command_string);
  return command_string;
}

}  // namespace delegate_execute
