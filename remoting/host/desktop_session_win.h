// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_WIN_H_
#define REMOTING_HOST_DESKTOP_SESSION_WIN_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/win/wts_console_observer.h"
#include "remoting/host/worker_process_ipc_delegate.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class DaemonProcess;
class WorkerProcessLauncher;
class WtsConsoleMonitor;

// DesktopSession implementation which attaches to the host's physical console.
// Receives IPC messages from the desktop process, running in the console
// session, via |WorkerProcessIpcDelegate|, and monitors console session
// attach/detach events via |WtsConsoleObserer|.
// TODO(alexeypa): replace |WtsConsoleObserver| with an interface capable of
// monitoring both the console and RDP connections. See http://crbug.com/137696.
class DesktopSessionWin
    : public DesktopSession,
      public WorkerProcessIpcDelegate,
      public WtsConsoleObserver {
 public:
  // Passes the owning |daemon_process|, a unique identifier of the desktop
  // session |id| and the interface for monitoring console session attach/detach
  // events. Both |daemon_process| and |monitor| must outlive |this|.
  DesktopSessionWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsConsoleMonitor* monitor);
  virtual ~DesktopSessionWin();

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnPermanentError() OVERRIDE;

  // WtsConsoleObserver implementation.
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 private:
  // Task runner on which public methods of this class should be called.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Message loop used by the IPC channel.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Contains the full path to the desktop binary.
  FilePath desktop_binary_;

  // Launches and monitors the desktop process.
  scoped_ptr<WorkerProcessLauncher> launcher_;

  // Pointer used to unsubscribe from session attach and detach events.
  WtsConsoleMonitor* monitor_;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_WIN_H_
