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
#include "base/single_thread_task_runner.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session_win.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/ipc_consts.h"
#include "remoting/host/win/host_service.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/unprivileged_process_delegate.h"
#include "remoting/host/win/worker_process_launcher.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace remoting {

class WtsConsoleMonitor;

class DaemonProcessWin : public DaemonProcess {
 public:
  DaemonProcessWin(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      const base::Closure& stopped_callback);
  virtual ~DaemonProcessWin();

  // Sends an IPC message to the worker process. This method can be called only
  // after successful Start() and until Stop() is called or an error occurred.
  virtual void SendToNetwork(IPC::Message* message) OVERRIDE;

 protected:
  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // DaemonProcess implementation.
  virtual scoped_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id) OVERRIDE;
  virtual void LaunchNetworkProcess() OVERRIDE;

 private:
  scoped_ptr<WorkerProcessLauncher> network_launcher_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcessWin);
};

DaemonProcessWin::DaemonProcessWin(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(caller_task_runner, io_task_runner, stopped_callback) {
}

DaemonProcessWin::~DaemonProcessWin() {
  // Make sure that the object is completely stopped. The same check exists
  // in Stoppable::~Stoppable() but this one helps us to fail early and
  // predictably.
  CHECK_EQ(stoppable_state(), Stoppable::kStopped);
}

void DaemonProcessWin::SendToNetwork(IPC::Message* message) {
  if (network_launcher_) {
    network_launcher_->Send(message);
  } else {
    delete message;
  }
}

void DaemonProcessWin::DoStop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  network_launcher_.reset();
  DaemonProcess::DoStop();
}

scoped_ptr<DesktopSession> DaemonProcessWin::DoCreateDesktopSession(
    int terminal_id) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return scoped_ptr<DesktopSession>(new DesktopSessionWin(
      caller_task_runner(), io_task_runner(), this, terminal_id,
      HostService::GetInstance()));
}

void DaemonProcessWin::LaunchNetworkProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!network_launcher_);

  // Construct the host binary name.
  FilePath host_binary;
  if (!GetInstalledBinaryPath(kHostBinaryName, &host_binary)) {
    Stop();
    return;
  }

  scoped_ptr<UnprivilegedProcessDelegate> delegate(
      new UnprivilegedProcessDelegate(caller_task_runner(), io_task_runner(),
                                      host_binary));
  network_launcher_.reset(new WorkerProcessLauncher(
      caller_task_runner(), delegate.Pass(), this));
}

scoped_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback) {
  scoped_ptr<DaemonProcessWin> daemon_process(
      new DaemonProcessWin(caller_task_runner, io_task_runner,
                           stopped_callback));
  daemon_process->Initialize();
  return daemon_process.PassAs<DaemonProcess>();
}

}  // namespace remoting
