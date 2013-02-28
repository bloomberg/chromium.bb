// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_CONSOLE_SESSION_PROCESS_DRIVER_H_
#define REMOTING_HOST_WIN_WTS_CONSOLE_SESSION_PROCESS_DRIVER_H_

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/base/stoppable.h"
#include "remoting/host/win/wts_terminal_observer.h"
#include "remoting/host/worker_process_ipc_delegate.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace remoting {

class SasInjector;
class WorkerProcessLauncher;
class WtsTerminalMonitor;

// Launches the host in the session attached to the console. When a new session
// attaches to the console relaunches the host in it.
class WtsConsoleSessionProcessDriver
    : public Stoppable,
      public WorkerProcessIpcDelegate,
      public WtsTerminalObserver {
 public:
  // Constructs a WtsConsoleSessionProcessDriver object. |stopped_callback| will
  // be invoked to notify the caller that the object is completely stopped. All
  // public methods of this class must be invoked on the |caller_task_runner|
  // thread. |monitor| will only be accessed from the |caller_task_runner|
  // thread. |io_task_runner| must be an I/O message loop.
  WtsConsoleSessionProcessDriver(
      const base::Closure& stopped_callback,
      WtsTerminalMonitor* monitor,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  virtual ~WtsConsoleSessionProcessDriver();

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnPermanentError() OVERRIDE;

  // WtsTerminalObserver implementation.
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 protected:
  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // Sends Secure Attention Sequence to the console session.
  void OnSendSasToConsole();

 private:
  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Message loop used by the IPC channel.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Launches and monitors the worker process.
  scoped_ptr<WorkerProcessLauncher> launcher_;

  // Used to unsubscribe from session attach and detach events.
  WtsTerminalMonitor* monitor_;

  scoped_ptr<SasInjector> sas_injector_;

  DISALLOW_COPY_AND_ASSIGN(WtsConsoleSessionProcessDriver);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_CONSOLE_SESSION_PROCESS_DRIVER_H_
