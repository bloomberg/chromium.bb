// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/host/win/worker_process_launcher.h"

namespace base {
class CommandLine;
class SingleThreadTaskRunner;
} // namespace base

namespace IPC {
class Message;
} // namespace base

namespace remoting {

// Implements logic for launching and monitoring a worker process in a different
// session.
class WtsSessionProcessDelegate
    : public base::NonThreadSafe,
      public WorkerProcessLauncher::Delegate {
 public:
  WtsSessionProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_ptr<base::CommandLine> target,
      bool launch_elevated,
      const std::string& channel_security);
  ~WtsSessionProcessDelegate();

  // Initializes the object returning true on success.
  bool Initialize(uint32 session_id);

  // WorkerProcessLauncher::Delegate implementation.
  virtual void LaunchProcess(WorkerProcessLauncher* event_handler) OVERRIDE;
  virtual void Send(IPC::Message* message) OVERRIDE;
  virtual void CloseChannel() OVERRIDE;
  virtual void KillProcess() OVERRIDE;

 private:
  // The actual implementation resides in WtsSessionProcessDelegate::Core class.
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
