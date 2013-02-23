// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/host/win/worker_process_launcher.h"

class CommandLine;

namespace base {
class FilePath;
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class Listener;
class Message;
} // namespace base

namespace remoting {

// Implements logic for launching and monitoring a worker process in a different
// session.
class WtsSessionProcessDelegate
    : public WorkerProcessLauncher::Delegate {
 public:
  WtsSessionProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_ptr<CommandLine> target,
      uint32 session_id,
      bool launch_elevated,
      const std::string& channel_security);
  ~WtsSessionProcessDelegate();

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
  // The actual implementation resides in WtsSessionProcessDelegate::Core class.
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
