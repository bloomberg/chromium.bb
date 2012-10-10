// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_
#define REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_handle.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class Message;
} // namespace IPC

namespace remoting {

class WorkerProcessIpcDelegate;

// Launches a worker process that is controlled via an IPC channel. All
// interaction with the spawned process is through WorkerProcessIpcDelegate and
// Send() method. In case of error the channel is closed and the worker process
// is terminated.
class WorkerProcessLauncher {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Returns the exit code of the worker process.
    virtual DWORD GetExitCode() = 0;

    // Terminates the worker process with the given exit code.
    virtual void KillProcess(DWORD exit_code) = 0;

    // Starts the worker process and passes |channel_name| to it.
    // |process_exit_event_out| receives a handle that becomes signalled once
    // the launched process has been terminated.
    virtual bool LaunchProcess(
        const std::string& channel_name,
        base::win::ScopedHandle* process_exit_event_out) = 0;
  };

  // Creates the launcher that will use |launcher_delegate| to manage the worker
  // process and |worker_delegate| to handle IPCs. The caller must ensure that
  // |worker_delegate| remains valid until Stoppable::Stop() method has been
  // called.
  //
  // The caller should call all the methods on this class on
  // the |caller_task_runner| thread. Methods of both delegate interfaces are
  // called on the |caller_task_runner| thread as well. |io_task_runner| is used
  // to perform background IPC I/O.
  WorkerProcessLauncher(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_ptr<Delegate> launcher_delegate,
      WorkerProcessIpcDelegate* worker_delegate,
      const std::string& pipe_security_descriptor);
  ~WorkerProcessLauncher();

  // Sends an IPC message to the worker process. The message will be silently
  // dropped if Send() is called before Start() or after stutdown has been
  // initiated.
  void Send(IPC::Message* message);

 private:
  // The actual implementation resides in WorkerProcessLauncher::Core class.
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WORKER_PROCESS_LAUNCHER_H_
