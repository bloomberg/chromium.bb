// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/wts_console_session_process_driver.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_session_process_delegate.h"
#include "remoting/host/win/wts_terminal_monitor.h"

// The security descriptor of the named pipe the process running in the console
// session connects to. It gives full access to LocalSystem and denies access by
// anyone else.
const char kDaemonIpcSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = {
    "host-config", switches::kV, switches::kVModule
};

namespace remoting {

WtsConsoleSessionProcessDriver::WtsConsoleSessionProcessDriver(
    const base::Closure& stopped_callback,
    WtsTerminalMonitor* monitor,
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : Stoppable(caller_task_runner, stopped_callback),
      caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      monitor_(monitor) {
  monitor_->AddWtsTerminalObserver(net::IPEndPoint(), this);
}

WtsConsoleSessionProcessDriver::~WtsConsoleSessionProcessDriver() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Make sure that the object is completely stopped. The same check exists
  // in Stoppable::~Stoppable() but this one helps us to fail early and
  // predictably.
  CHECK_EQ(stoppable_state(), Stoppable::kStopped);

  monitor_->RemoveWtsTerminalObserver(this);

  CHECK(launcher_.get() == NULL);
}

void WtsConsoleSessionProcessDriver::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
}

bool WtsConsoleSessionProcessDriver::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WtsConsoleSessionProcessDriver, message)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_SendSasToConsole,
                        OnSendSasToConsole)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

  // Launch elevated on Win8 to be able to inject Alt+Tab.
  bool launch_elevated = base::win::GetVersion() >= base::win::VERSION_WIN8;

  // Get the name of the executable to run. |kDesktopBinaryName| specifies
  // uiAccess="true" in it's manifest.
  base::FilePath desktop_binary;
  bool result;
  if (launch_elevated) {
    result = GetInstalledBinaryPath(kDesktopBinaryName, &desktop_binary);
  } else {
    result = GetInstalledBinaryPath(kHostBinaryName, &desktop_binary);
  }

  if (!result) {
    Stop();
    return;
  }

  scoped_ptr<CommandLine> target(new CommandLine(desktop_binary));
  target->AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeHost);
  target->CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                           kCopiedSwitchNames,
                           arraysize(kCopiedSwitchNames));

  // Create a Delegate capable of launching an elevated process in the session.
  scoped_ptr<WtsSessionProcessDelegate> delegate(
      new WtsSessionProcessDelegate(io_task_runner_,
                                    target.Pass(),
                                    launch_elevated,
                                    kDaemonIpcSecurityDescriptor));
  if (!delegate->Initialize(session_id)) {
    Stop();
    return;
  }

  // Use the Delegate to launch the host process.
  launcher_.reset(new WorkerProcessLauncher(delegate.Pass(), this));
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

void WtsConsoleSessionProcessDriver::OnSendSasToConsole() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!launcher_)
    return;

  if (!sas_injector_)
    sas_injector_ = SasInjector::Create();
  if (!sas_injector_->InjectSas())
    LOG(ERROR) << "Failed to inject Secure Attention Sequence.";
}

} // namespace remoting
