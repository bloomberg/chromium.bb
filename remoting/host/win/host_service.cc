// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/host_service.h"

#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/win/wrapped_window_proc.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/base/stoppable.h"
#include "remoting/host/branding.h"

#if defined(REMOTING_MULTI_PROCESS)
#include "remoting/host/daemon_process.h"
#endif  // defined(REMOTING_MULTI_PROCESS)

#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/host_service_resource.h"
#include "remoting/host/win/wts_console_observer.h"

#if !defined(REMOTING_MULTI_PROCESS)
#include "remoting/host/win/wts_session_process_launcher.h"
#endif  // !defined(REMOTING_MULTI_PROCESS)

using base::StringPrintf;

namespace {

// Session id that does not represent any session.
const uint32 kInvalidSessionId = 0xffffffffu;

const char kIoThreadName[] = "I/O thread";

// A window class for the session change notifications window.
const wchar_t kSessionNotificationWindowClass[] =
  L"Chromoting_SessionNotificationWindow";

// Command line switches:

// "--console" runs the service interactively for debugging purposes.
const char kConsoleSwitchName[] = "console";

// "--elevate=<binary>" requests <binary> to be launched elevated, presenting
// a UAC prompt if necessary.
const char kElevateSwitchName[] = "elevate";

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

// Exit codes:
const int kSuccessExitCode = 0;
const int kUsageExitCode = 1;
const int kErrorExitCode = 2;

void usage(const FilePath& program_name) {
  LOG(INFO) << StringPrintf(kUsageMessage,
                            UTF16ToWide(program_name.value()).c_str());
}

void QuitMessageLoop(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // namespace

namespace remoting {

HostService::HostService() :
  console_session_id_(kInvalidSessionId),
  run_routine_(&HostService::RunAsService),
  service_status_handle_(0),
  stopped_event_(true, false) {
}

HostService::~HostService() {
}

void HostService::AddWtsConsoleObserver(WtsConsoleObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  console_observers_.AddObserver(observer);
}

void HostService::RemoveWtsConsoleObserver(WtsConsoleObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  console_observers_.RemoveObserver(observer);
}

void HostService::OnChildStopped() {
  child_.reset(NULL);
  main_task_runner_ = NULL;
}

void HostService::OnSessionChange() {
  // WTSGetActiveConsoleSessionId is a very cheap API. It basically reads
  // a single value from shared memory. Therefore it is better to check if
  // the console session is still the same every time a session change
  // notification event is posted. This also takes care of coalescing multiple
  // events into one since we look at the latest state.
  uint32 console_session_id = WTSGetActiveConsoleSessionId();
  if (console_session_id_ != console_session_id) {
    if (console_session_id_ != kInvalidSessionId) {
      FOR_EACH_OBSERVER(WtsConsoleObserver,
                        console_observers_,
                        OnSessionDetached());
    }

    console_session_id_ = console_session_id;

    if (console_session_id_ != kInvalidSessionId) {
      FOR_EACH_OBSERVER(WtsConsoleObserver,
                        console_observers_,
                        OnSessionAttached(console_session_id_));
    }
  }
}

BOOL WINAPI HostService::ConsoleControlHandler(DWORD event) {
  HostService* self = HostService::GetInstance();
  switch (event) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      self->main_task_runner_->PostTask(FROM_HERE, base::Bind(
          &Stoppable::Stop, base::Unretained(self->child_.get())));
      self->stopped_event_.Wait();
      return TRUE;

    default:
      return FALSE;
  }
}

HostService* HostService::GetInstance() {
  return Singleton<HostService>::get();
}

bool HostService::InitWithCommandLine(const CommandLine* command_line) {
  CommandLine::StringVector args = command_line->GetArgs();

  // Check if launch with elevation was requested.
  if (command_line->HasSwitch(kElevateSwitchName)) {
    run_routine_ = &HostService::Elevate;
    return true;
  }

  if (!args.empty()) {
    LOG(ERROR) << "No positional parameters expected.";
    return false;
  }

  // Run interactively if needed.
  if (run_routine_ == &HostService::RunAsService &&
      command_line->HasSwitch(kConsoleSwitchName)) {
    run_routine_ = &HostService::RunInConsole;
  }

  return true;
}

int HostService::Run() {
  return (this->*run_routine_)();
}

void HostService::CreateLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {

#if defined(REMOTING_MULTI_PROCESS)

  child_ = DaemonProcess::Create(
      main_task_runner_,
      io_task_runner,
      base::Bind(&HostService::OnChildStopped,
                 base::Unretained(this))).PassAs<Stoppable>();

#else  // !defined(REMOTING_MULTI_PROCESS)

  // Create the session process launcher.
  child_.reset(new WtsSessionProcessLauncher(
      base::Bind(&HostService::OnChildStopped, base::Unretained(this)),
      this,
      main_task_runner_,
      io_task_runner));

#endif  // !defined(REMOTING_MULTI_PROCESS)
}

void HostService::RunMessageLoop(MessageLoop* message_loop) {
  // Launch the I/O thread.
  base::Thread io_thread(kIoThreadName);
  base::Thread::Options io_thread_options(MessageLoop::TYPE_IO, 0);
  if (!io_thread.StartWithOptions(io_thread_options)) {
    LOG(FATAL) << "Failed to start the I/O thread";
    return;
  }

  CreateLauncher(new AutoThreadTaskRunner(io_thread.message_loop_proxy(),
                                          main_task_runner_));

  // Run the service.
  message_loop->Run();
}

int HostService::Elevate() {
  // Get the name of the binary to launch.
  FilePath binary =
      CommandLine::ForCurrentProcess()->GetSwitchValuePath(kElevateSwitchName);

  // Create the child process command line by copying known switches from our
  // command line.
  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kCopiedSwitchNames,
                                _countof(kCopiedSwitchNames));
  CommandLine::StringType parameters = command_line.GetCommandLineString();

  // Launch the child process requesting elevation.
  SHELLEXECUTEINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.lpVerb = L"runas";
  info.lpFile = binary.value().c_str();
  info.lpParameters = parameters.c_str();
  info.nShow = SW_SHOWNORMAL;

  if (!ShellExecuteEx(&info)) {
    return GetLastError();
  }

  return kSuccessExitCode;
}

int HostService::RunAsService() {
  SERVICE_TABLE_ENTRYW dispatch_table[] = {
    { const_cast<LPWSTR>(kWindowsServiceName), &HostService::ServiceMain },
    { NULL, NULL }
  };

  if (!StartServiceCtrlDispatcherW(dispatch_table)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to connect to the service control manager";
    return kErrorExitCode;
  }

  return kSuccessExitCode;
}

int HostService::RunInConsole() {
  MessageLoop message_loop(MessageLoop::TYPE_UI);

  // Keep a reference to the main message loop while it is used. Once the last
  // reference is dropped, QuitClosure() will be posted to the loop.
  main_task_runner_ =
      new AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                               base::Bind(&QuitMessageLoop, &message_loop));

  int result = kErrorExitCode;

  // Subscribe to Ctrl-C and other console events.
  if (!SetConsoleCtrlHandler(&HostService::ConsoleControlHandler, TRUE)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to set console control handler";
    return result;
  }

  // Create a window for receiving session change notifications.
  HWND window = NULL;
  WNDCLASSEX window_class;
  base::win::InitializeWindowClass(
      kSessionNotificationWindowClass,
      &base::win::WrappedWindowProc<SessionChangeNotificationProc>,
      0, 0, 0, NULL, NULL, NULL, NULL, NULL,
      &window_class);
  HINSTANCE instance = window_class.hInstance;
  ATOM atom = RegisterClassExW(&window_class);
  if (atom == 0) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to register the window class '"
        << kSessionNotificationWindowClass << "'";
    goto cleanup;
  }

  window = CreateWindowW(MAKEINTATOM(atom), 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                         instance, 0);
  if (window == NULL) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to creat the session notificationwindow";
    goto cleanup;
  }

  // Post a dummy session change notification to peek up the current console
  // session.
  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &HostService::OnSessionChange, base::Unretained(this)));

  // Subscribe to session change notifications.
  if (WTSRegisterSessionNotification(window,
                                     NOTIFY_FOR_ALL_SESSIONS) != FALSE) {
    // Run the service.
    RunMessageLoop(&message_loop);

    // Release the control handler.
    stopped_event_.Signal();

    WTSUnRegisterSessionNotification(window);
    result = kSuccessExitCode;
  }

cleanup:
  if (window != NULL) {
    DestroyWindow(window);
  }

  if (atom != 0) {
    UnregisterClass(MAKEINTATOM(atom), instance);
  }

  // Unsubscribe from console events. Ignore the exit code. There is nothing
  // we can do about it now and the program is about to exit anyway. Even if
  // it crashes nothing is going to be broken because of it.
  SetConsoleCtrlHandler(&HostService::ConsoleControlHandler, FALSE);

  return result;
}

DWORD WINAPI HostService::ServiceControlHandler(DWORD control,
                                                DWORD event_type,
                                                LPVOID event_data,
                                                LPVOID context) {
  HostService* self = reinterpret_cast<HostService*>(context);
  switch (control) {
    case SERVICE_CONTROL_INTERROGATE:
      return NO_ERROR;

    case SERVICE_CONTROL_SHUTDOWN:
    case SERVICE_CONTROL_STOP:
      self->main_task_runner_->PostTask(FROM_HERE, base::Bind(
          &Stoppable::Stop, base::Unretained(self->child_.get())));
      self->stopped_event_.Wait();
      return NO_ERROR;

    case SERVICE_CONTROL_SESSIONCHANGE:
      self->main_task_runner_->PostTask(FROM_HERE, base::Bind(
          &HostService::OnSessionChange, base::Unretained(self)));
      return NO_ERROR;

    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
}

VOID WINAPI HostService::ServiceMain(DWORD argc, WCHAR* argv[]) {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);

  // Keep a reference to the main message loop while it is used. Once the last
  // reference is dropped QuitClosure() will be posted to the loop.
  HostService* self = HostService::GetInstance();
  self->main_task_runner_ =
      new AutoThreadTaskRunner(message_loop.message_loop_proxy(),
                               base::Bind(&QuitMessageLoop, &message_loop));

  // Register the service control handler.
  self->service_status_handle_ =
      RegisterServiceCtrlHandlerExW(kWindowsServiceName,
                                    &HostService::ServiceControlHandler,
                                    self);
  if (self->service_status_handle_ == 0) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to register the service control handler";
    return;
  }

  // Report running status of the service.
  SERVICE_STATUS service_status;
  ZeroMemory(&service_status, sizeof(service_status));
  service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status.dwCurrentState = SERVICE_RUNNING;
  service_status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN |
                                      SERVICE_ACCEPT_STOP |
                                      SERVICE_ACCEPT_SESSIONCHANGE;
  service_status.dwWin32ExitCode = kSuccessExitCode;

  if (!SetServiceStatus(self->service_status_handle_, &service_status)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to report service status to the service control manager";
    return;
  }

  // Post a dummy session change notification to peek up the current console
  // session.
  self->main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &HostService::OnSessionChange, base::Unretained(self)));

  // Run the service.
  self->RunMessageLoop(&message_loop);

  // Release the control handler.
  self->stopped_event_.Signal();

  // Tell SCM that the service is stopped.
  service_status.dwCurrentState = SERVICE_STOPPED;
  service_status.dwControlsAccepted = 0;

  if (!SetServiceStatus(self->service_status_handle_, &service_status)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to report service status to the service control manager";
    return;
  }
}

LRESULT CALLBACK HostService::SessionChangeNotificationProc(HWND hwnd,
                                                            UINT message,
                                                            WPARAM wparam,
                                                            LPARAM lparam) {
  switch (message) {
    case WM_WTSSESSION_CHANGE: {
      HostService* self = HostService::GetInstance();
      self->OnSessionChange();
      return 0;
    }

    default:
      return DefWindowProc(hwnd, message, wparam, lparam);
  }
}

} // namespace remoting

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE previous_instance,
                     LPSTR raw_command_line,
                     int show_command) {
#ifdef OFFICIAL_BUILD
  if (remoting::IsUsageStatsAllowed()) {
    remoting::InitializeCrashReporting();
  }
#endif  // OFFICIAL_BUILD

  // CommandLine::Init() ignores the passed |argc| and |argv| on Windows getting
  // the command line from GetCommandLineW(), so we can safely pass NULL here.
  CommandLine::Init(0, NULL);

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  // Write logs to the application profile directory.
  FilePath debug_log = remoting::GetConfigDir().
      Append(FILE_PATH_LITERAL("debug.log"));
  InitLogging(debug_log.value().c_str(),
              logging::LOG_ONLY_TO_FILE,
              logging::DONT_LOCK_LOG_FILE,
              logging::APPEND_TO_OLD_LOG_FILE,
              logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    usage(command_line->GetProgram());
    return kSuccessExitCode;
  }

  remoting::HostService* service = remoting::HostService::GetInstance();
  if (!service->InitWithCommandLine(command_line)) {
    usage(command_line->GetProgram());
    return kUsageExitCode;
  }

  return service->Run();
}
