// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "tools/accessibility/inspect/ax_event_server.h"

char kPidSwitch[] = "pid";

int main(int argc, char** argv) {
  base::AtExitManager at_exit_manager;
  // TODO(aleventhal) Want callback after Ctrl+C or some global keystroke:
  // base::AtExitManager::RegisterCallback(content::OnExit, nullptr);

  base::CommandLine::Init(argc, argv);
  std::string pid_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kPidSwitch);
  int pid = 0;
  if (!pid_str.empty())
    base::StringToInt(pid_str, &pid);

  std::unique_ptr<content::AXEventServer> server(
      new content::AXEventServer(pid));
  return 0;
}
