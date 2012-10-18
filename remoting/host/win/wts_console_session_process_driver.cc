// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_console_session_process_driver.h"

#include "base/base_switches.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "ipc/ipc_message.h"
#include "remoting/host/ipc_consts.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_console_monitor.h"
#include "remoting/host/win/wts_session_process_delegate.h"

// The security descriptor of the named pipe the process running in the console
// session connects to. It gives full access to LocalSystem and denies access by
// anyone else.
const char kDaemonIpcSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

namespace remoting {

WtsConsoleSessionProcessDriver::WtsConsoleSessionProcessDriver(
    const base::Closure& stopped_callback,
    WtsConsoleMonitor* monitor,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : Stoppable(caller_task_runner, stopped_callback),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      monitor_(monitor) {
  monitor_->AddWtsConsoleObserver(this);
}

WtsConsoleSessionProcessDriver::~WtsConsoleSessionProcessDriver() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Make sure that the object is completely stopped. The same check exists
  // in Stoppable::~Stoppable() but this one helps us to fail early and
  // predictably.
  CHECK_EQ(stoppable_state(), Stoppable::kStopped);

  monitor_->RemoveWtsConsoleObserver(this);

  CHECK(launcher_.get() == NULL);
}

void WtsConsoleSessionProcessDriver::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

bool WtsConsoleSessionProcessDriver::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return false;
}

void WtsConsoleSessionProcessDriver::OnPermanentError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  Stop();
}

void WtsConsoleSessionProcessDriver::OnSessionAttached(uint32 session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (stoppable_state() != Stoppable::kRunning) {
    return;
  }

  DCHECK(launcher_.get() == NULL);

  // Construct the host binary name.
  FilePath host_binary;
  if (!GetInstalledBinaryPath(kHostBinaryName, &host_binary)) {
    Stop();
    return;
  }

  // Create a Delegate capable of launching an elevated process in the session.
  scoped_ptr<WtsSessionProcessDelegate> delegate(
      new WtsSessionProcessDelegate(caller_task_runner_,
                                    io_task_runner_,
                                    host_binary,
                                    session_id,
                                    true,
                                    kDaemonIpcSecurityDescriptor));

  // Use the Delegate to launch the host process.
  launcher_.reset(new WorkerProcessLauncher(
      caller_task_runner_, delegate.Pass(), this));
}

void WtsConsoleSessionProcessDriver::OnSessionDetached() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(launcher_.get() != NULL);

  launcher_.reset();
}

void WtsConsoleSessionProcessDriver::DoStop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launcher_.reset();
  CompleteStopping();
}

} // namespace remoting
