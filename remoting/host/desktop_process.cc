// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/desktop_process.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "remoting/host/branding.h"
#include "remoting/host/constants.h"
#include "remoting/host/usage_stats_consent.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include <commctrl.h>
#endif  // defined(OS_WIN)

namespace {

// "--help" or "--?" prints the usage message.
const char kHelpSwitchName[] = "help";
const char kQuestionSwitchName[] = "?";

const wchar_t kUsageMessage[] =
  L"\n"
  L"Usage: %ls [options]\n"
  L"\n"
  L"Options:\n"
  L"  --help, --?     - Print this message.\n";

void usage(const FilePath& program_name) {
  LOG(INFO) << StringPrintf(kUsageMessage,
                            UTF16ToWide(program_name.value()).c_str());
}

}  // namespace

namespace remoting {

DesktopProcess::DesktopProcess() {
}

DesktopProcess::~DesktopProcess() {
}

int DesktopProcess::Run() {
  NOTIMPLEMENTED();
  return 0;
}

} // namespace remoting

int main(int argc, char** argv) {
#if defined(OS_MACOSX)
  // Needed so we don't leak objects when threads are created.
  base::mac::ScopedNSAutoreleasePool pool;
#endif

  CommandLine::Init(argc, argv);

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  // Initialize logging with an appropriate log-file location, and default to
  // log to that on Windows, or to standard error output otherwise.
  FilePath debug_log = remoting::GetConfigDir().
      Append(FILE_PATH_LITERAL("debug.log"));
  InitLogging(debug_log.value().c_str(),
#if defined(OS_WIN)
              logging::LOG_ONLY_TO_FILE,
#else
              logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
#endif
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(command_line->GetProgram());
    return remoting::kSuccessExitCode;
  }

  remoting::DesktopProcess desktop_process;
  return desktop_process.Run();
}

#if defined(OS_WIN)

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE previous_instance,
                     LPSTR raw_command_line,
                     int show_command) {
#ifdef OFFICIAL_BUILD
  if (remoting::IsUsageStatsAllowed()) {
    remoting::InitializeCrashReporting();
  }
#endif  // OFFICIAL_BUILD

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

  // CommandLine::Init() ignores the passed |argc| and |argv| on Windows getting
  // the command line from GetCommandLineW(), so we can safely pass NULL here.
  return main(0, NULL);
}

#endif  // defined(OS_WIN)
