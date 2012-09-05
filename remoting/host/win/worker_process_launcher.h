// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_
#define REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "base/timer.h"
#include "base/win/scoped_handle.h"
#include "base/win/object_watcher.h"
#include "ipc/ipc_channel.h"
#include "remoting/base/stoppable.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class ChannelProxy;
class Message;
} // namespace IPC

namespace remoting {

class WorkerProcessIpcDelegate;

// Launches a worker process that is controlled via an IPC channel. All
// interaction with the spawned process is through the IPC::Listener and Send()
// method. In case of error the channel is closed and the worker process is
// terminated.
//
// WorkerProcessLauncher object is good for one process launch attempt only.
class WorkerProcessLauncher
    : public Stoppable,
      public base::win::ObjectWatcher::Delegate,
      public IPC::Listener {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Starts the worker process and passes |channel_name| to it.
    // |process_exit_event_out| receives a handle that becomes signalled once
    // the launched process has been terminated.
    virtual bool DoLaunchProcess(
        const std::string& channel_name,
        base::win::ScopedHandle* process_exit_event_out) = 0;

    // Terminates the worker process with the given exit code.
    virtual void DoKillProcess(DWORD exit_code) = 0;
  };

  // Creates the launcher that will use |launcher_delegate| to manage the worker
  // process and |worker_delegate| to handle IPCs.
  //
  // |stopped_callback| and |main_task_runner| are passed to the underlying
  // |Stoppable| implementation. The caller should call all the methods on this
  // class on the |main_task_runner| thread. |ipc_task_runner| is used to
  // perform background IPC I/O.
  WorkerProcessLauncher(
      Delegate* launcher_delegate,
      WorkerProcessIpcDelegate* worker_delegate,
      const base::Closure& stopped_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);
  virtual ~WorkerProcessLauncher();

  // Starts the worker process.
  void Start(const std::string& pipe_sddl);

  // Sends an IPC message to the worker process. The message will be silently
  // dropped if Send() is called before Start() or after stutdown has been
  // initiated.
  void Send(IPC::Message* message);

  // base::win::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 protected:
  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

 private:
  // Creates the server end of the Chromoting IPC channel.
  bool CreatePipeForIpcChannel(const std::string& channel_name,
                               const std::string& pipe_sddl,
                               base::win::ScopedHandle* pipe_out);

  // Generates random channel ID.
  std::string GenerateRandomChannelId();

  Delegate* launcher_delegate_;
  WorkerProcessIpcDelegate* worker_delegate_;

  // The main service message loop.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Message loop used by the IPC channel.
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // Used to determine when the launched process terminates.
  base::win::ObjectWatcher process_watcher_;

  // A waiting handle that becomes signalled once the launched process has
  // been terminated.
  base::win::ScopedHandle process_exit_event_;

  // The IPC channel connecting to the launched process.
  scoped_ptr<IPC::ChannelProxy> ipc_channel_;

  // The timer used to delay termination of the process in the case of an IPC
  // error.
  base::OneShotTimer<WorkerProcessLauncher> ipc_error_timer_;

  // The server end of the pipe.
  base::win::ScopedHandle pipe_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_
