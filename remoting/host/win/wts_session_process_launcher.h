// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_PROCESS_LAUNCHER_H_
#define REMOTING_HOST_WIN_WTS_SESSION_PROCESS_LAUNCHER_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel.h"
#include "remoting/base/stoppable.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_console_observer.h"
#include "remoting/host/worker_process_ipc_delegate.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace remoting {

class WtsConsoleMonitor;

class WtsSessionProcessLauncher
    : public base::MessagePumpForIO::IOHandler,
      public Stoppable,
      public WorkerProcessIpcDelegate,
      public WorkerProcessLauncher::Delegate,
      public WtsConsoleObserver {
 public:
  // Constructs a WtsSessionProcessLauncher object. |stopped_callback| and
  // |main_message_loop| are passed to the undelying |Stoppable| implementation.
  // All interaction with |monitor| should happen on |main_message_loop|.
  // |ipc_message_loop| must be an I/O message loop.
  WtsSessionProcessLauncher(
      const base::Closure& stopped_callback,
      WtsConsoleMonitor* monitor,
      scoped_refptr<base::SingleThreadTaskRunner> main_message_loop,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_message_loop);

  virtual ~WtsSessionProcessLauncher();

  // base::MessagePumpForIO::IOHandler implementation.
  virtual void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                             DWORD bytes_transferred,
                             DWORD error) OVERRIDE;

  // WorkerProcessIpcDelegate implementation.
  virtual void OnChannelConnected() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WorkerProcessLauncher::Delegate implementation.
  virtual bool DoLaunchProcess(
      const std::string& channel_name,
      base::win::ScopedHandle* process_exit_event_out) OVERRIDE;
  virtual void DoKillProcess(DWORD exit_code) OVERRIDE;

  // WtsConsoleObserver implementation.
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 protected:
  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

 private:
  // Drains the completion port queue to make sure that all job object
  // notifications has been received.
  void DrainJobNotifications();

  // Notifies that the completion port queue has been drained.
  void DrainJobNotificationsCompleted();

  // Creates and initializes the job object that will sandbox the launched child
  // processes.
  void InitializeJob();

  // Notifies that the job object initialization is complete.
  void InitializeJobCompleted(scoped_ptr<base::win::ScopedHandle> job);

  void OnJobNotification(DWORD message, DWORD pid);

  // Attempts to launch the host process in the current console session.
  // Schedules next launch attempt if creation of the process fails for any
  // reason.
  void LaunchProcess();

  // Called when the launcher reports the process to be stopped.
  void OnLauncherStopped();

  // |true| if this object is currently attached to the console session.
  bool attached_;

  // Time of the last launch attempt.
  base::Time launch_time_;

  // Current backoff delay.
  base::TimeDelta launch_backoff_;

  // Timer used to schedule the next attempt to launch the process.
  base::OneShotTimer<WtsSessionProcessLauncher> timer_;

  // The main service message loop.
  scoped_refptr<base::SingleThreadTaskRunner> main_message_loop_;

  // Message loop used by the IPC channel.
  scoped_refptr<base::SingleThreadTaskRunner> ipc_message_loop_;

  // This pointer is used to unsubscribe from session attach and detach events.
  WtsConsoleMonitor* monitor_;

  scoped_ptr<WorkerProcessLauncher> launcher_;

  // The job object used to control the lifetime of child processes.
  base::win::ScopedHandle job_;

  // A waiting handle that becomes signalled once all process associated with
  // the job have been terminated.
  base::win::ScopedHandle process_exit_event_;

  enum JobState {
    kJobUninitialized,
    kJobRunning,
    kJobStopping,
    kJobStopped
  };

  JobState job_state_;

  base::win::ScopedHandle worker_process_;

  // The token to be used to launch a process in a different session.
  base::win::ScopedHandle session_token_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_PROCESS_LAUNCHER_H_
