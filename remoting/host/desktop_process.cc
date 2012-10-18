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
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_channel_proxy.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"
#include "remoting/host/usage_stats_consent.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
#include <commctrl.h>
#endif  // defined(OS_WIN)

namespace {

// The command line switch specifying the name of the daemon IPC endpoint.
const char kDaemonIpcSwitchName[] = "daemon-pipe";

const char kIoThreadName[] = "I/O thread";

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

DesktopProcess::DesktopProcess(const std::string& daemon_channel_name)
    : daemon_channel_name_(daemon_channel_name) {
}

DesktopProcess::~DesktopProcess() {
}

bool DesktopProcess::OnMessageReceived(const IPC::Message& message) {
  return false;
}

void DesktopProcess::OnChannelConnected(int32 peer_pid) {
  NOTIMPLEMENTED();
}

void DesktopProcess::OnChannelError() {
  LOG(ERROR) << "Failed to connect to '" << daemon_channel_name_ << "'";
  daemon_channel_.reset();
}

int DesktopProcess::Run() {
  // Create the UI message loop.
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  {
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
        new remoting::AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                                           MessageLoop::QuitClosure());

    // Launch the I/O thread.
    scoped_refptr<AutoThreadTaskRunner> io_task_runner =
        AutoThread::CreateWithType(kIoThreadName, ui_task_runner,
                                   MessageLoop::TYPE_IO);

    // Connect to the daemon.
    daemon_channel_.reset(new IPC::ChannelProxy(daemon_channel_name_,
                                                IPC::Channel::MODE_CLIENT,
                                                this,
                                                io_task_runner));
  }

  // Run the UI message loop.
  base::RunLoop run_loop;
  run_loop.Run();
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

  remoting::InitHostLogging();

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(command_line->GetProgram());
    return remoting::kSuccessExitCode;
  }

  std::string channel_name =
      command_line->GetSwitchValueASCII(kDaemonIpcSwitchName);

  if (channel_name.empty()) {
    usage(command_line->GetProgram());
    return remoting::kUsageExitCode;
  }

  remoting::DesktopProcess desktop_process(channel_name);
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
