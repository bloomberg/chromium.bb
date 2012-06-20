// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_SERVICE_WIN_H_
#define REMOTING_HOST_HOST_SERVICE_WIN_H_

#include <windows.h>

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"

#include "remoting/host/wts_console_monitor_win.h"

class CommandLine;
class MessageLoop;

namespace remoting {

class WtsConsoleObserver;

class HostService : public WtsConsoleMonitor {
 public:
  static HostService* GetInstance();

  // This function parses the command line and selects the action routine.
  bool InitWithCommandLine(const CommandLine* command_line);

  // Invoke the choosen action routine.
  int Run();

  // WtsConsoleMonitor implementation
  virtual void AddWtsConsoleObserver(WtsConsoleObserver* observer) OVERRIDE;
  virtual void RemoveWtsConsoleObserver(
                   WtsConsoleObserver* observer) OVERRIDE;

 private:
  HostService();
  ~HostService();

  // Notifies the service of changes in session state.
  void OnSessionChange();

  // This is a common entry point to the main service loop called by both
  // RunAsService() and RunInConsole().
  void RunMessageLoop();

  // This function handshakes with the service control manager and starts
  // the service.
  int RunAsService();

  // This function starts the service in interactive mode (i.e. as a plain
  // console application).
  int RunInConsole();

  static BOOL WINAPI ConsoleControlHandler(DWORD event);

  // The control handler of the service.
  static DWORD WINAPI ServiceControlHandler(DWORD control,
                                            DWORD event_type,
                                            LPVOID event_data,
                                            LPVOID context);

  // The main service entry point.
  static VOID WINAPI ServiceMain(DWORD argc, WCHAR* argv[]);

  static LRESULT CALLBACK SessionChangeNotificationProc(HWND hwnd,
                                                        UINT message,
                                                        WPARAM wparam,
                                                        LPARAM lparam);

  // Current physical console session id.
  uint32 console_session_id_;

  // The list of observers receiving notifications about any session attached
  // to the physical console.
  ObserverList<WtsConsoleObserver> console_observers_;

  // The host binary name.
  FilePath host_binary_;

  // Service message loop.
  MessageLoop* message_loop_;

  // The action routine to be executed.
  int (HostService::*run_routine_)();

  // The service name.
  std::wstring service_name_;

  // The service status handle.
  SERVICE_STATUS_HANDLE service_status_handle_;

  // True if the service is being stopped.
  bool shutting_down_;

  // A waitable event that is used to wait until the service is stopped.
  base::WaitableEvent stopped_event_;

  // Singleton.
  friend struct DefaultSingletonTraits<HostService>;

  DISALLOW_COPY_AND_ASSIGN(HostService);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_SERVICE_WIN_H_
