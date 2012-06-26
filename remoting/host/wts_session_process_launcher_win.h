// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_
#define REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/win/scoped_handle.h"
#include "base/win/object_watcher.h"
#include "ipc/ipc_channel.h"

#include "remoting/host/wts_console_observer_win.h"

namespace base {
class MessageLoopProxy;
} // namespace base

namespace IPC {
class ChannelProxy;
class Message;
} // namespace IPC

namespace remoting {

// Session id that does not represent any session.
extern const uint32 kInvalidSessionId;

class SasInjector;
class WtsConsoleMonitor;

class WtsSessionProcessLauncher
    : public base::win::ObjectWatcher::Delegate,
      public IPC::Channel::Listener,
      public WtsConsoleObserver {
 public:
  // Constructs a WtsSessionProcessLauncher object. |host_binary| is the name of
  // the executable to be launched in the console session. All interaction with
  // |monitor| should happen on |main_message_loop|. |ipc_message_loop| has
  // to be an I/O message loop.
  WtsSessionProcessLauncher(
      WtsConsoleMonitor* monitor,
      const FilePath& host_binary,
      scoped_refptr<base::MessageLoopProxy> main_message_loop,
      scoped_refptr<base::MessageLoopProxy> ipc_message_loop);

  virtual ~WtsSessionProcessLauncher();

  // base::win::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WtsConsoleObserver implementation.
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 private:
  // Attempts to launch the host process in the current console session.
  // Schedules next launch attempt if creation of the process fails for any
  // reason.
  void LaunchProcess();

  // Sends the Secure Attention Sequence to the session represented by
  // |session_token_|.
  void OnSendSasToConsole();

  // Name of the host executable.
  FilePath host_binary_;

  // Time of the last launch attempt.
  base::Time launch_time_;

  // Current backoff delay.
  base::TimeDelta launch_backoff_;

  // Timer used to schedule the next attempt to launch the process.
  base::OneShotTimer<WtsSessionProcessLauncher> timer_;

  // The main service message loop.
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  // Message loop used by the IPC channel.
  scoped_refptr<base::MessageLoopProxy> ipc_message_loop_;

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

  // The Chromoting IPC channel connecting the service to the per-session
  // process.
  scoped_ptr<IPC::ChannelProxy> chromoting_channel_;

  scoped_ptr<SasInjector> sas_injector_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_
