// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_listener.h"
#include "remoting/host/win/worker_process_launcher.h"

namespace base {
class CommandLine;
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class ChannelProxy;
class Message;
} // namespace IPC

namespace remoting {

// Implements logic for launching and monitoring a worker process under a less
// privileged user account.
class UnprivilegedProcessDelegate
    : public base::NonThreadSafe,
      public IPC::Listener,
      public WorkerProcessLauncher::Delegate {
 public:
  UnprivilegedProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_ptr<base::CommandLine> target_command);
  virtual ~UnprivilegedProcessDelegate();

  // WorkerProcessLauncher::Delegate implementation.
  virtual void LaunchProcess(WorkerProcessLauncher* event_handler) OVERRIDE;
  virtual void Send(IPC::Message* message) OVERRIDE;
  virtual void CloseChannel() OVERRIDE;
  virtual void KillProcess() OVERRIDE;

 private:
  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  void ReportFatalError();
  void ReportProcessLaunched(base::win::ScopedHandle worker_process);

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Command line of the launched process.
  scoped_ptr<base::CommandLine> target_command_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  scoped_ptr<IPC::ChannelProxy> channel_;

  WorkerProcessLauncher* event_handler_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  DISALLOW_COPY_AND_ASSIGN(UnprivilegedProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
