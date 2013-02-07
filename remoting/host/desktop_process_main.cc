// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/desktop_process_main.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/host/basic_desktop_environment.h"
#include "remoting/host/desktop_process.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/session_desktop_environment.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include <commctrl.h>
#endif  // defined(OS_WIN)

namespace {

// The command line switch specifying the name of the daemon IPC endpoint.
const char kDaemonIpcSwitchName[] = "daemon-pipe";

// "--help" or "--?" prints the usage message.
const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";

const char kUsageMessage[] =
  "\n"
  "Usage: %s [options]\n"
  "\n"
  "Options:\n"
  "  --help, --?     - Print this message.\n";

void Usage(const base::FilePath& program_name) {
  std::string display_name = UTF16ToUTF8(program_name.LossyDisplayName());
  LOG(INFO) << StringPrintf(kUsageMessage, display_name.c_str());
}

}  // namespace

namespace remoting {

int DesktopProcessMain(int argc, char** argv) {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  CommandLine::Init(argc, argv);

  // Initialize Breakpad as early as possible. On Mac the command-line needs to
  // be initialized first, so that the preference for crash-reporting can be
  // looked up in the config file.
#if defined(OFFICIAL_BUILD) && (defined(MAC_BREAKPAD) || defined(OS_WIN))
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // defined(OFFICIAL_BUILD) && (defined(MAC_BREAKPAD) || defined(OS_WIN))

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  InitHostLogging();

#if defined(OS_WIN)
  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);

  // Mark the process as DPI-aware, so Windows won't scale coordinates in APIs.
  // N.B. This API exists on Vista and above.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    FilePath path(base::GetNativeLibraryName(UTF8ToUTF16("user32")));
    base::ScopedNativeLibrary user32(path);
    CHECK(user32.is_valid());

    typedef BOOL (WINAPI * SetProcessDPIAwareFn)();
    SetProcessDPIAwareFn set_process_dpi_aware =
        static_cast<SetProcessDPIAwareFn>(
            user32.GetFunctionPointer("SetProcessDPIAware"));
    set_process_dpi_aware();
  }
#endif  // defined(OS_WIN)

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    Usage(command_line->GetProgram());
    return kSuccessExitCode;
  }

  std::string channel_name =
      command_line->GetSwitchValueASCII(kDaemonIpcSwitchName);

  if (channel_name.empty()) {
    Usage(command_line->GetProgram());
    return kUsageExitCode;
  }

  MessageLoop message_loop(MessageLoop::TYPE_UI);
  base::RunLoop run_loop;
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
      new AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                               run_loop.QuitClosure());

  DesktopProcess desktop_process(ui_task_runner, channel_name);

  // Create a platform-dependent environment factory.
  scoped_ptr<DesktopEnvironmentFactory> desktop_environment_factory;
#if defined(OS_WIN)
  desktop_environment_factory.reset(
      new SessionDesktopEnvironmentFactory(
          base::Bind(&DesktopProcess::InjectSas,
                     desktop_process.AsWeakPtr())));
#else  // !defined(OS_WIN)
  desktop_environment_factory.reset(
      new BasicDesktopEnvironmentFactory(true));
#endif  // !defined(OS_WIN)

  if (!desktop_process.Start(desktop_environment_factory.Pass()))
    return kInitializationFailed;

  // Run the UI message loop.
  ui_task_runner = NULL;
  run_loop.Run();

  return kSuccessExitCode;
}

}  // namespace remoting

#if !defined(OS_WIN)
int main(int argc, char** argv) {
  return remoting::DesktopProcessMain(argc, argv);
}
#endif  // !defined(OS_WIN)
