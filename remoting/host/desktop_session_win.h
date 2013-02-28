// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_WIN_H_
#define REMOTING_HOST_DESKTOP_SESSION_WIN_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/win/wts_terminal_observer.h"
#include "remoting/host/worker_process_ipc_delegate.h"

namespace tracked_objects {
class Location;
}  // namespace tracked_objects

namespace remoting {

class AutoThreadTaskRunner;
class DaemonProcess;
class SasInjector;
class WorkerProcessLauncher;
class WtsTerminalMonitor;

// DesktopSession implementation which attaches to the host's physical console.
// Receives IPC messages from the desktop process, running in the console
// session, via |WorkerProcessIpcDelegate|, and monitors console session
// attach/detach events via |WtsConsoleObserer|.
// TODO(alexeypa): replace |WtsTerminalObserver| with an interface capable of
// monitoring both the console and RDP connections. See http://crbug.com/137696.
class DesktopSessionWin
    : public DesktopSession,
      public WorkerProcessIpcDelegate,
      public WtsTerminalObserver {
 public:
  // Passes the owning |daemon_process|, a unique identifier of the desktop
  // session |id| and the interface for monitoring console session attach/detach
  // events. Both |daemon_process| and |monitor| must outlive |this|.
  DesktopSessionWin(
    scoped_refptr<AutoThreadTaskRunner> main_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor);
  virtual ~DesktopSessionWin();

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnPermanentError() OVERRIDE;

  // WtsTerminalObserver implementation.
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 private:
  // ChromotingDesktopDaemonMsg_DesktopAttached handler.
  void OnDesktopSessionAgentAttached(IPC::PlatformFileForTransit desktop_pipe);

  // ChromotingDesktopDaemonMsg_InjectSas handler.
  void OnInjectSas();

  // Restarts the desktop process.
  void RestartDesktopProcess(const tracked_objects::Location& location);

  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;

  // Message loop used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Contains the full path to the desktop binary.
  base::FilePath desktop_binary_;

  // Handle of the desktop process.
  base::win::ScopedHandle desktop_process_;

  // Launches and monitors the desktop process.
  scoped_ptr<WorkerProcessLauncher> launcher_;

  // Pointer used to unsubscribe from session attach and detach events.
  WtsTerminalMonitor* monitor_;

  scoped_ptr<SasInjector> sas_injector_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_WIN_H_
