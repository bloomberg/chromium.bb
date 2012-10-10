// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/win/worker_process_launcher.h"

class FilePath;

namespace base {
class SingleThreadTaskRunner;
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
      const FilePath& binary_path,
      uint32 session_id,
      bool launch_elevated);
  ~WtsSessionProcessDelegate();

  // WorkerProcessLauncher::Delegate implementation.
  virtual DWORD GetExitCode() OVERRIDE;
  virtual void KillProcess(DWORD exit_code) OVERRIDE;
  virtual bool LaunchProcess(
      const std::string& channel_name,
      base::win::ScopedHandle* process_exit_event_out) OVERRIDE;

 private:
  // The actual implementation resides in WtsSessionProcessDelegate::Core class.
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_SESSION_PROCESS_DELEGATE_H_
