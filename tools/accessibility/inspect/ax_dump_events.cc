// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "tools/accessibility/inspect/ax_event_server.h"

namespace {

constexpr char kPidSwitch[] = "pid";

// Convert from string to int, whether in 0x hex format or decimal format.
bool StringToInt(std::string str, int* result) {
  if (str.empty())
    return false;
  bool is_hex =
      str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
  return is_hex ? base::HexStringToInt(str, result)
                : base::StringToInt(str, result);
}

bool AXDumpEventsLogMessageHandler(int severity,
                                   const char* file,
                                   int line,
                                   size_t message_start,
                                   const std::string& str) {
  printf("%s", str.substr(message_start).c_str());
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  logging::SetLogMessageHandler(AXDumpEventsLogMessageHandler);

  base::CommandLine::Init(argc, argv);
  const std::string pid_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kPidSwitch);
  int pid;
  if (pid_str.empty() || !StringToInt(pid_str, &pid)) {
    LOG(ERROR) << "* Error: No process id provided via --pid=[process-id].";
    return 1;
  }

  base::AtExitManager exit_manager;
  base::MessageLoopForUI message_loop;
  const auto server = std::make_unique<tools::AXEventServer>(pid);
  base::RunLoop().Run();

  return 0;
}
