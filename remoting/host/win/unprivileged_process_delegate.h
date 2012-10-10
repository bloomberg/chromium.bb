// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "remoting/host/win/worker_process_launcher.h"

namespace base {
class SingleThreadTaskRunner;
} // namespace base

namespace remoting {

// Implements logic for launching and monitoring a worker process under a less
// privileged user account.
class UnprivilegedProcessDelegate : public WorkerProcessLauncher::Delegate {
 public:
  UnprivilegedProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const FilePath& binary_path);
  virtual ~UnprivilegedProcessDelegate();

  // WorkerProcessLauncher::Delegate implementation.
  virtual DWORD GetExitCode() OVERRIDE;
  virtual void KillProcess(DWORD exit_code) OVERRIDE;
  virtual bool LaunchProcess(
      const std::string& channel_name,
      base::win::ScopedHandle* process_exit_event_out) OVERRIDE;

 private:
  // The task runner all public methods of this class should be called on.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Path to the worker process binary.
  FilePath binary_path_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  DISALLOW_COPY_AND_ASSIGN(UnprivilegedProcessDelegate);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
