// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_WIN_H_
#define REMOTING_HOST_DESKTOP_SESSION_WIN_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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
class DesktopSession;
class ScreenResolution;
class WorkerProcessLauncher;
class WtsTerminalMonitor;

// DesktopSession implementation which attaches to either physical or virtual
// (RDP) console. Receives IPC messages from the desktop process, running in
// the target session, via |WorkerProcessIpcDelegate|, and monitors session
// attach/detach events via |WtsTerminalObserer|.
class DesktopSessionWin
    : public DesktopSession,
      public WorkerProcessIpcDelegate,
      public WtsTerminalObserver {
 public:
  // Creates a desktop session instance that attaches to the physical console.
  static scoped_ptr<DesktopSession> CreateForConsole(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      DaemonProcess* daemon_process,
      int id,
      const ScreenResolution& resolution);

  // Creates a desktop session instance that attaches to a virtual console.
  static scoped_ptr<DesktopSession> CreateForVirtualTerminal(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      DaemonProcess* daemon_process,
      int id,
      const ScreenResolution& resolution);

 protected:
  // Passes the owning |daemon_process|, a unique identifier of the desktop
  // session |id| and the interface for monitoring session attach/detach events.
  // Both |daemon_process| and |monitor| must outlive |this|.
  DesktopSessionWin(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsTerminalMonitor* monitor);
  virtual ~DesktopSessionWin();

  const scoped_refptr<AutoThreadTaskRunner>& caller_task_runner() const {
    return caller_task_runner_;
  }

  // Called when |session_attach_timer_| expires.
  void OnSessionAttachTimeout();

  // Starts monitoring for session attach/detach events for |terminal_id|.
  void StartMonitoring(const std::string& terminal_id);

  // Stops monitoring for session attach/detach events.
  void StopMonitoring();

  // Asks DaemonProcess to terminate this session.
  void TerminateSession();

  // Injects a secure attention sequence into the session.
  virtual void InjectSas() = 0;

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnPermanentError(int exit_code) OVERRIDE;

  // WtsTerminalObserver implementation.
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 private:
  // ChromotingDesktopDaemonMsg_DesktopAttached handler.
  void OnDesktopSessionAgentAttached(IPC::PlatformFileForTransit desktop_pipe);

  // Requests the desktop process to crash.
  void CrashDesktopProcess(const tracked_objects::Location& location);

  // Reports time elapsed since previous event to the debug log.
  void ReportElapsedTime(const std::string& event);

  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Message loop used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Handle of the desktop process (running an instance of DesktopSessionAgent).
  base::win::ScopedHandle desktop_process_;

  // Launches and monitors the desktop process.
  scoped_ptr<WorkerProcessLauncher> launcher_;

  // Used to unsubscribe from session attach and detach events.
  WtsTerminalMonitor* monitor_;

  // True if |this| is subsribed to receive session attach/detach notifications.
  bool monitoring_notifications_;

  // Used to report an error if the session attach notification does not arrives
  // for too long.
  base::OneShotTimer<DesktopSessionWin> session_attach_timer_;

  base::Time last_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_WIN_H_
