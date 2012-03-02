// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_
#define REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/compiler_specific.h"
#include "base/process.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/win/scoped_handle.h"
#include "base/win/object_watcher.h"

#include "remoting/host/wts_console_observer_win.h"

namespace remoting {

class WtsConsoleMonitor;

class WtsSessionProcessLauncher
    : public base::win::ObjectWatcher::Delegate,
      public WtsConsoleObserver {
 public:
  // Constructs a WtsSessionProcessLauncher object. |monitor| must outlive this
  // class. |host_binary| is the name of the executable to be launched in
  // the console session.
  WtsSessionProcessLauncher(WtsConsoleMonitor* monitor,
                            const FilePath& host_binary);

  virtual ~WtsSessionProcessLauncher();

  // base::win::ObjectWatcher::Delegate implementation
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // WtsConsoleObserver implementation
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 private:
  // Attempts to launch the host process in the current console session.
  // Schedules next launch attempt if creation of the process fails for any
  // reason.
  void LaunchProcess();

  // Name of the host executable.
  FilePath host_binary_;

  // Time of the last launch attempt.
  base::Time launch_time_;

  // Current backoff delay.
  base::TimeDelta launch_backoff_;

  // Timer used to schedule the next attempt to launch the process.
  base::OneShotTimer<WtsSessionProcessLauncher> timer_;

  // This pointer is used to unsubscribe from session attach and detach events.
  WtsConsoleMonitor* monitor_;

  // Impersonation token that has the SE_TCB_NAME privilege enabled.
  base::win::ScopedHandle privileged_token_;

  // The handle of the process injected into the console session.
  base::Process process_;

  // Used to determine when the launched process terminates.
  base::win::ObjectWatcher process_watcher_;

  // The token to be used to launch a process in a different session.
  base::win::ScopedHandle session_token_;

  // Defines the states the process launcher can be in.
  enum State {
    StateDetached,
    StateStarting,
    StateAttached,
  };

  // Current state of the process launcher.
  State state_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_
