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
#include "base/win/scoped_handle.h"
#include "remoting/host/win/worker_process_launcher.h"

class CommandLine;

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class ChannelProxy;
class Listener;
class Message;
} // namespace IPC

namespace remoting {

// Implements logic for launching and monitoring a worker process under a less
// privileged user account.
class UnprivilegedProcessDelegate : public WorkerProcessLauncher::Delegate {
 public:
  UnprivilegedProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_ptr<CommandLine> target_command);
  virtual ~UnprivilegedProcessDelegate();

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // WorkerProcessLauncher::Delegate implementation.
  virtual DWORD GetProcessId() const OVERRIDE;
  virtual bool IsPermanentError(int failure_count) const OVERRIDE;
  virtual void KillProcess(DWORD exit_code) OVERRIDE;
  virtual bool LaunchProcess(
      IPC::Listener* delegate,
      base::win::ScopedHandle* process_exit_event_out) OVERRIDE;

 private:
  // The task runner all public methods of this class should be called on.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Command line of the launched process.
  scoped_ptr<CommandLine> target_command_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  scoped_ptr<IPC::ChannelProxy> channel_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  DISALLOW_COPY_AND_ASSIGN(UnprivilegedProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
