// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/unprivileged_process_delegate.h"
#include "remoting/host/win/worker_process_launcher.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

const FilePath::CharType kMe2meHostBinaryName[] =
    FILE_PATH_LITERAL("remoting_host.exe");

// The security descriptor of the daemon IPC endpoint. It gives full access
// to LocalSystem and denies access by anyone else.
const char kDaemonPipeSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

} // namespace

namespace remoting {

class DaemonProcessWin : public DaemonProcess {
 public:
  DaemonProcessWin(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                   const base::Closure& stopped_callback);
  virtual ~DaemonProcessWin();

  // Sends an IPC message to the worker process. This method can be called only
  // after successful Start() and until Stop() is called or an error occurred.
  virtual void Send(IPC::Message* message) OVERRIDE;

 protected:
  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // DaemonProcess implementation.
  virtual void LaunchNetworkProcess() OVERRIDE;

 private:
  scoped_ptr<WorkerProcessLauncher> launcher_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcessWin);
};

DaemonProcessWin::DaemonProcessWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(main_task_runner, io_task_runner, stopped_callback) {
}

DaemonProcessWin::~DaemonProcessWin() {
  // Make sure that the object is completely stopped. The same check exists
  // in Stoppable::~Stoppable() but this one helps us to fail early and
  // predictably.
  CHECK_EQ(stoppable_state(), Stoppable::kStopped);
}

void DaemonProcessWin::LaunchNetworkProcess() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());
  DCHECK(launcher_.get() == NULL);

  // Construct the host binary name.
  FilePath dir_path;
  if (!PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    Stop();
    return;
  }

  scoped_ptr<UnprivilegedProcessDelegate> delegate(
      new UnprivilegedProcessDelegate(main_task_runner(), io_task_runner(),
                                      dir_path.Append(kMe2meHostBinaryName)));
  launcher_.reset(new WorkerProcessLauncher(main_task_runner(),
                                            io_task_runner(),
                                            delegate.Pass(),
                                            this,
                                            kDaemonPipeSecurityDescriptor));
}

void DaemonProcessWin::Send(IPC::Message* message) {
  if (launcher_.get() != NULL) {
    launcher_->Send(message);
  } else {
    delete message;
  }
}

void DaemonProcessWin::DoStop() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());

  launcher_.reset();
  DaemonProcess::DoStop();
}

scoped_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback) {
  scoped_ptr<DaemonProcessWin> daemon_process(
      new DaemonProcessWin(main_task_runner, io_task_runner, stopped_callback));
  return daemon_process.PassAs<DaemonProcess>();
}

}  // namespace remoting
