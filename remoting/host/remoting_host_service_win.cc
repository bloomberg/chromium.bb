// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include <windows.h>
#include <stdio.h>

#include "base/command_line.h"
#include "base/logging.h"

namespace {

// Command line actions and switches:
// "run" sumply runs the service as usual.
const wchar_t kRunActionName[] = L"run";

// "install" requests the service to be installed.
const wchar_t kInstallActionName[] = L"install";

// "remove" uninstalls the service.
const wchar_t kRemoveActionName[] = L"remove";

// "--console" runs the service interactively for debugging purposes.
const char kConsoleSwitchName[] = "console";

// "--help" or "--?" prints the usage message.
const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";

const char kUsageMessage[] =
  "\n"
  "Usage: %s [action] [options]\n"
  "\n"
  "Actions:\n"
  "  run         - Run the service. If no action specified 'run' is assumed.\n"
  "  install     - Install the service.\n"
  "  remove      - Uninstall the service.\n"
  "\n"
  "Options:\n"
  "  --console   - Run the service interactively for debugging purposes.\n"
  "  --help, --? - Print this message.\n";

// Exit codes:
const int kSuccessExitCode = 0;
const int kUsageExitCode = 1;

void usage(const char* program_name) {
  fprintf(stderr, kUsageMessage, program_name);
}

}  // namespace

namespace remoting {

class HostService {
 public:
  HostService():
    run_routine_(&HostService::RunAsService) {
  }

  ~HostService() {
  }

  // This function parses the command line and selects the action routine.
  bool InitWithCommandLine(const CommandLine* command_line) {
    CommandLine::StringVector args = command_line->GetArgs();

    // Choose the action to perform.
    if (!args.empty()) {
      if (args.size() > 1) {
        LOG(ERROR) << "Invalid command line: more than one action requested.";
        return false;
      }
      if (args[0] == kInstallActionName) {
        run_routine_ = &HostService::Install;
      } else if (args[0] == kRemoveActionName) {
        run_routine_ = &HostService::Remove;
      } else if (args[0] != kRunActionName) {
        LOG(ERROR) << "Invalid command line: invalid action specified: "
                   << args[0];
        return false;
      }
    }

    // Run interactively if needed.
    if (run_routine_ == &HostService::RunAsService &&
        command_line->HasSwitch(kConsoleSwitchName)) {
      run_routine_ = &HostService::RunInConsole;
    }

    return true;
  }

  // Invoke the choosen action routine.
  int Run() {
    return (this->*run_routine_)();
  }

 private:
  int Install() {
    NOTIMPLEMENTED();
    return 0;
  }

  int Remove() {
    NOTIMPLEMENTED();
    return 0;
  }

  int RunAsService() {
    NOTIMPLEMENTED();
    return 0;
  }

  int RunInConsole() {
    NOTIMPLEMENTED();
    return 0;
  }

  int (HostService::*run_routine_)();
};

} // namespace remoting

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(argv[0]);
    return kSuccessExitCode;
  }

  remoting::HostService service;
  if (!service.InitWithCommandLine(command_line)) {
    usage(argv[0]);
    return kUsageExitCode;
  }

  return service.Run();
}
