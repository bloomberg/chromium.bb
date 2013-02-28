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
#include "base/scoped_native_library.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
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
#include "remoting/host/win/wts_terminal_observer.h"

#if !defined(REMOTING_MULTI_PROCESS)
#include "remoting/host/win/wts_console_session_process_driver.h"
#endif  // !defined(REMOTING_MULTI_PROCESS)

namespace {

// Used to query the endpoint of an attached RDP client.
const WINSTATIONINFOCLASS kWinStationRemoteAddress =
    static_cast<WINSTATIONINFOCLASS>(29);

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

bool HostService::AddWtsTerminalObserver(const net::IPEndPoint& client_endpoint,
                                        WtsTerminalObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  RegisteredObserver registered_observer;
  registered_observer.client_endpoint = client_endpoint;
  registered_observer.session_id = kInvalidSessionId;
  registered_observer.observer = observer;

  bool session_id_found = false;
  std::list<RegisteredObserver>::const_iterator i;
  for (i = observers_.begin(); i != observers_.end(); ++i) {
    // Get the attached session ID from another observer watching the same WTS
    // console if any.
    if (i->client_endpoint == client_endpoint) {
      registered_observer.session_id = i->session_id;
      session_id_found = true;
    }

    // Check that |observer| hasn't been registered already.
    if (i->observer == observer)
      return false;
  }

  // If |client_endpoint| is new, enumerate all sessions to see if there is one
  // attached to |client_endpoint|.
  if (!session_id_found)
    registered_observer.session_id = GetSessionIdForEndpoint(client_endpoint);

  observers_.push_back(registered_observer);

  if (registered_observer.session_id != kInvalidSessionId) {
    observer->OnSessionAttached(registered_observer.session_id);
  }

  return true;
}

void HostService::RemoveWtsTerminalObserver(WtsTerminalObserver* observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  std::list<RegisteredObserver>::const_iterator i;
  for (i = observers_.begin(); i != observers_.end(); ++i) {
    if (i->observer == observer) {
      observers_.erase(i);
      return;
    }
  }
}

HostService::HostService() :
  win_station_query_information_(NULL),
  run_routine_(&HostService::RunAsService),
  service_status_handle_(0),
  stopped_event_(true, false) {
}

HostService::~HostService() {
}

bool HostService::GetEndpointForSessionId(uint32 session_id,
                                          net::IPEndPoint* endpoint) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Fast path for the case when |session_id| is currently attached to
  // the physical console.
  if (session_id == WTSGetActiveConsoleSessionId()) {
    *endpoint = net::IPEndPoint();
    return true;
  }

  // Get the pointer to winsta!WinStationQueryInformationW().
  if (!LoadWinStationLibrary())
    return false;

  // WinStationRemoteAddress information class returns the following structure.
  // Note that its layout is different from sockaddr_in/sockaddr_in6. For
  // instance both |ipv4| and |ipv6| structures are 4 byte aligned so there is
  // additional 2 byte padding after |sin_family|.
  struct RemoteAddress {
    unsigned short sin_family;
    union {
      struct {
        USHORT sin_port;
        ULONG in_addr;
        UCHAR sin_zero[8];
      } ipv4;
      struct {
        USHORT sin6_port;
        ULONG sin6_flowinfo;
        USHORT sin6_addr[8];
        ULONG sin6_scope_id;
      } ipv6;
    };
  };

  RemoteAddress address;
  ULONG length;
  if (!win_station_query_information_(WTS_CURRENT_SERVER_HANDLE,
                                      session_id,
                                      kWinStationRemoteAddress,
                                      &address,
                                      sizeof(address),
                                      &length)) {
    // WinStationQueryInformationW() fails if no RDP client is attached to
    // |session_id|.
    return false;
  }

  // Convert the RemoteAddress structure into sockaddr_in/sockaddr_in6.
  switch (address.sin_family) {
    case AF_INET: {
      sockaddr_in ipv4 = { 0 };
      ipv4.sin_family = AF_INET;
      ipv4.sin_port = address.ipv4.sin_port;
      ipv4.sin_addr.S_un.S_addr = address.ipv4.in_addr;
      return endpoint->FromSockAddr(
          reinterpret_cast<struct sockaddr*>(&ipv4), sizeof(ipv4));
    }

    case AF_INET6: {
      sockaddr_in6 ipv6 = { 0 };
      ipv6.sin6_family = AF_INET6;
      ipv6.sin6_port = address.ipv6.sin6_port;
      ipv6.sin6_flowinfo = address.ipv6.sin6_flowinfo;
      memcpy(&ipv6.sin6_addr, address.ipv6.sin6_addr, sizeof(ipv6.sin6_addr));
      ipv6.sin6_scope_id = address.ipv6.sin6_scope_id;
      return endpoint->FromSockAddr(
          reinterpret_cast<struct sockaddr*>(&ipv6), sizeof(ipv6));
    }

    default:
      return false;
  }
}

uint32 HostService::GetSessionIdForEndpoint(
    const net::IPEndPoint& client_endpoint) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Use the fast path if the caller wants to get id of the session attached to
  // the physical console.
  if (client_endpoint == net::IPEndPoint())
    return WTSGetActiveConsoleSessionId();

  // Get the pointer to winsta!WinStationQueryInformationW().
  if (!LoadWinStationLibrary())
    return kInvalidSessionId;

  // Enumerate all sessions and try to match the client endpoint.
  WTS_SESSION_INFO* session_info;
  DWORD session_info_count;
  if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &session_info,
                            &session_info_count)) {
    LOG_GETLASTERROR(ERROR) << "Failed to enumerate all sessions";
    return kInvalidSessionId;
  }
  for (DWORD i = 0; i < session_info_count; ++i) {
    net::IPEndPoint endpoint;
    if (GetEndpointForSessionId(session_info[i].SessionId, &endpoint) &&
        endpoint == client_endpoint) {
      WTSFreeMemory(session_info);
      return session_info[i].SessionId;
    }
  }

  // |client_endpoint| is not associated with any session.
  WTSFreeMemory(session_info);
  return kInvalidSessionId;
}

bool HostService::LoadWinStationLibrary() {
  if (!winsta_) {
    base::FilePath winsta_path(base::GetNativeLibraryName(
        UTF8ToUTF16("winsta")));
    winsta_.reset(new base::ScopedNativeLibrary(winsta_path));

    if (winsta_->is_valid()) {
      win_station_query_information_ =
          static_cast<PWINSTATIONQUERYINFORMATIONW>(
              winsta_->GetFunctionPointer("WinStationQueryInformationW"));
    }
  }

  return win_station_query_information_ != NULL;
}

void HostService::OnSessionChange(uint32 event, uint32 session_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(session_id, kInvalidSessionId);

  // Process only attach/detach notifications.
  if (event != WTS_CONSOLE_CONNECT && event != WTS_CONSOLE_DISCONNECT &&
      event != WTS_REMOTE_CONNECT && event != WTS_REMOTE_DISCONNECT) {
    return;
  }

  // Assuming that notification can arrive later query the current state of
  // |session_id|.
  net::IPEndPoint client_endpoint;
  bool attached = GetEndpointForSessionId(session_id, &client_endpoint);

  std::list<RegisteredObserver>::iterator i = observers_.begin();
  while (i != observers_.end()) {
    std::list<RegisteredObserver>::iterator next = i;
    ++next;

    // Issue a detach notification if the session was detached from a client or
    // if it is now attached to a different client.
    if (i->session_id == session_id &&
        (!attached || !(i->client_endpoint == client_endpoint))) {
      i->session_id = kInvalidSessionId;
      i->observer->OnSessionDetached();
      i = next;
      continue;
    }

    // The client currently attached to |session_id| was attached to a different
    // session before. Reconnect it to |session_id|.
    if (attached && i->client_endpoint == client_endpoint &&
        i->session_id != session_id) {
      WtsTerminalObserver* observer = i->observer;

      if (i->session_id != kInvalidSessionId) {
        i->session_id = kInvalidSessionId;
        i->observer->OnSessionDetached();
      }

      // Verify that OnSessionDetached() above didn't remove |observer|
      // from the list.
      std::list<RegisteredObserver>::iterator j = next;
      --j;
      if (j->observer == observer) {
        j->session_id = session_id;
        observer->OnSessionAttached(session_id);
      }
    }

    i = next;
  }
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

void HostService::OnChildStopped() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  child_.reset(NULL);
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

// static
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

// static
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
          &HostService::OnSessionChange, base::Unretained(self), event_type,
          reinterpret_cast<WTSSESSION_NOTIFICATION*>(event_data)->dwSessionId));
      return NO_ERROR;

    default:
      return ERROR_CALL_NOT_IMPLEMENTED;
  }
}

// static
VOID WINAPI HostService::ServiceMain(DWORD argc, WCHAR* argv[]) {
  HostService* self = HostService::GetInstance();

  // Run the service.
  self->RunAsServiceImpl();

  // Release the control handler and notify the main thread that it can exit
  // now.
  self->stopped_event_.Signal();
}

// static
LRESULT CALLBACK HostService::SessionChangeNotificationProc(HWND hwnd,
                                                            UINT message,
                                                            WPARAM wparam,
                                                            LPARAM lparam) {
  switch (message) {
    case WM_WTSSESSION_CHANGE: {
      HostService* self = HostService::GetInstance();
      self->OnSessionChange(wparam, lparam);
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
