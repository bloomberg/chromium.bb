// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/daemon_process_main.h"

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "remoting/base/breakpad.h"
#include "remoting/host/branding.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/host_service.h"

using base::StringPrintf;

namespace {

// Command line switches:

// "--help" or "--?" prints the usage message.
const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";

const wchar_t kUsageMessage[] =
  L"\n"
  L"Usage: %ls [options]\n"
  L"\n"
  L"Options:\n"
  L"  --console       - Run the service interactively for debugging purposes.\n"
  L"  --elevate=<...> - Run <...> elevated.\n"
  L"  --help, --?     - Print this message.\n";

// The command line parameters that should be copied from the service's command
// line when launching an elevated child.
const char* kCopiedSwitchNames[] = {
    "host-config", "daemon-pipe", switches::kV, switches::kVModule };

void usage(const FilePath& program_name) {
  LOG(INFO) << StringPrintf(kUsageMessage,
                            UTF16ToWide(program_name.value()).c_str());
}

}  // namespace

namespace remoting {

int DaemonProcessMain() {
#ifdef OFFICIAL_BUILD
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // OFFICIAL_BUILD

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop, Singleton, etc).
  base::AtExitManager exit_manager;

  // CommandLine::Init() ignores the passed |argc| and |argv| on Windows getting
  // the command line from GetCommandLineW(), so we can safely pass NULL here.
  CommandLine::Init(0, NULL);

  InitHostLogging();

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(command_line->GetProgram());
    return kSuccessExitCode;
  }

  HostService* service = HostService::GetInstance();
  if (!service->InitWithCommandLine(command_line)) {
    usage(command_line->GetProgram());
    return kUsageExitCode;
  }

  return service->Run();
}

} // namespace remoting
