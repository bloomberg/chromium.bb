// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/host_service.h"

#include <windows.h>
#include <wtsapi32.h>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/win/wrapped_window_proc.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/base/stoppable.h"
#include "remoting/host/branding.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/logging.h"

#if defined(REMOTING_MULTI_PROCESS)
#include "remoting/host/daemon_process.h"
#endif  // defined(REMOTING_MULTI_PROCESS)

#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/wts_console_observer.h"

#if !defined(REMOTING_MULTI_PROCESS)
#include "remoting/host/win/wts_console_session_process_driver.h"
#endif  // !defined(REMOTING_MULTI_PROCESS)

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
  if (console_session_id_ != kInvalidSessionId)
    observer->OnSessionAttached(console_session_id_);
}

void HostService::RemoveWtsConsoleObserver(WtsConsoleObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  console_observers_.RemoveObserver(observer);
}

void HostService::OnChildStopped() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  child_.reset(NULL);
}

void HostService::OnSessionChange() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

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
    scoped_refptr<AutoThreadTaskRunner> task_runner) {
  // Launch the I/O thread.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner =
      AutoThread::CreateWithType(kIoThreadName, task_runner,
                                 MessageLoop::TYPE_IO);
  if (!io_task_runner) {
    LOG(FATAL) << "Failed to start the I/O thread";
    return;
  }

#if defined(REMOTING_MULTI_PROCESS)

  child_ = DaemonProcess::Create(
      task_runner,
      io_task_runner,
      base::Bind(&HostService::OnChildStopped,
                 base::Unretained(this))).PassAs<Stoppable>();

#else  // !defined(REMOTING_MULTI_PROCESS)

  // Create the console session process driver.
  child_.reset(new WtsConsoleSessionProcessDriver(
      base::Bind(&HostService::OnChildStopped, base::Unretained(this)),
      this,
      task_runner,
      io_task_runner));

#endif  // !defined(REMOTING_MULTI_PROCESS)
}

int HostService::RunAsService() {
  SERVICE_TABLE_ENTRYW dispatch_table[] = {
    { const_cast<LPWSTR>(kWindowsServiceName), &HostService::ServiceMain },
    { NULL, NULL }
  };

  if (!StartServiceCtrlDispatcherW(dispatch_table)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to connect to the service control manager";
    return kInitializationFailed;
  }

  // Wait until the service thread completely exited to avoid concurrent
  // teardown of objects registered with base::AtExitManager and object
  // destoyed by the service thread.
  stopped_event_.Wait();

  return kSuccessExitCode;
}

void HostService::RunAsServiceImpl() {
  MessageLoop message_loop(MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;
  main_task_runner_ = message_loop.message_loop_proxy();

  // Register the service control handler.
  service_status_handle_ = RegisterServiceCtrlHandlerExW(
      kWindowsServiceName, &HostService::ServiceControlHandler, this);
  if (service_status_handle_ == 0) {
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
  if (!SetServiceStatus(service_status_handle_, &service_status)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to report service status to the service control manager";
    return;
  }

  // Peek up the current console session.
  console_session_id_ = WTSGetActiveConsoleSessionId();

  CreateLauncher(scoped_refptr<AutoThreadTaskRunner>(
      new AutoThreadTaskRunner(main_task_runner_,
                               run_loop.QuitClosure())));

  // Run the service.
  run_loop.Run();

  // Tell SCM that the service is stopped.
  service_status.dwCurrentState = SERVICE_STOPPED;
  service_status.dwControlsAccepted = 0;
  if (!SetServiceStatus(service_status_handle_, &service_status)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to report service status to the service control manager";
    return;
  }
}

int HostService::RunInConsole() {
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  base::RunLoop run_loop;
  main_task_runner_ = message_loop.message_loop_proxy();

  int result = kInitializationFailed;

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

  // Subscribe to session change notifications.
  if (WTSRegisterSessionNotification(window,
                                     NOTIFY_FOR_ALL_SESSIONS) != FALSE) {
    // Peek up the current console session.
    console_session_id_ = WTSGetActiveConsoleSessionId();

    CreateLauncher(scoped_refptr<AutoThreadTaskRunner>(
        new AutoThreadTaskRunner(main_task_runner_,
                                 run_loop.QuitClosure())));

    // Run the service.
    run_loop.Run();

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
  HostService* self = HostService::GetInstance();

  // Run the service.
  self->RunAsServiceImpl();

  // Release the control handler and notify the main thread that it can exit
  // now.
  self->stopped_event_.Signal();
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

int DaemonProcessMain() {
  HostService* service = HostService::GetInstance();
  if (!service->InitWithCommandLine(CommandLine::ForCurrentProcess())) {
    return kUsageExitCode;
  }

  return service->Run();
}

} // namespace remoting
