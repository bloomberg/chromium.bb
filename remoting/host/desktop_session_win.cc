// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_win.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_console_monitor.h"
#include "remoting/host/win/wts_session_process_delegate.h"

using base::win::ScopedHandle;

namespace {

// The security descriptor of the daemon IPC endpoint. It gives full access
// to LocalSystem and denies access by anyone else.
const char kDaemonIpcSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = { switches::kV, switches::kVModule };

} // namespace

namespace remoting {

DesktopSessionWin::DesktopSessionWin(
    scoped_refptr<AutoThreadTaskRunner> main_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    DaemonProcess* daemon_process,
    int id,
    WtsConsoleMonitor* monitor)
    : DesktopSession(daemon_process, id),
      main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner),
      monitor_(monitor) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  monitor_->AddWtsConsoleObserver(this);
}

DesktopSessionWin::~DesktopSessionWin() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  launcher_.reset();
  monitor_->RemoveWtsConsoleObserver(this);
}

void DesktopSessionWin::OnChannelConnected(int32 peer_pid) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Obtain the handle of the desktop process. It will be passed to the network
  // process so it would be able to duplicate handles of shared memory objects
  // from the desktop process.
  desktop_process_.Set(OpenProcess(PROCESS_DUP_HANDLE, false, peer_pid));
  if (!desktop_process_.IsValid()) {
    RestartDesktopProcess(FROM_HERE);
    return;
  }

  VLOG(1) << "IPC: daemon <- desktop (" << peer_pid << ")";
}

bool DesktopSessionWin::OnMessageReceived(const IPC::Message& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DesktopSessionWin, message)
    IPC_MESSAGE_HANDLER(ChromotingDesktopDaemonMsg_DesktopAttached,
                        OnDesktopSessionAgentAttached)
    IPC_MESSAGE_HANDLER(ChromotingDesktopDaemonMsg_InjectSas,
                        OnInjectSas)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DesktopSessionWin::OnPermanentError() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  launcher_.reset();

  // This call will delete |this| so it should be at the very end of the method.
  daemon_process()->CloseDesktopSession(id());
}

void DesktopSessionWin::OnSessionAttached(uint32 session_id) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(launcher_.get() == NULL);

  // Get the desktop binary name.
  base::FilePath desktop_binary;
  if (!GetInstalledBinaryPath(kDesktopBinaryName, &desktop_binary)) {
    OnPermanentError();
    return;
  }

  scoped_ptr<CommandLine> target(new CommandLine(desktop_binary));
  target->AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeDesktop);
  target->CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                           kCopiedSwitchNames,
                           arraysize(kCopiedSwitchNames));

  // Create a delegate to launch a process into the session.
  scoped_ptr<WtsSessionProcessDelegate> delegate(
      new WtsSessionProcessDelegate(main_task_runner_, io_task_runner_,
                                    target.Pass(), session_id,
                                    true, kDaemonIpcSecurityDescriptor));

  // Create a launcher for the desktop process, using the per-session delegate.
  launcher_.reset(new WorkerProcessLauncher(
      main_task_runner_, delegate.Pass(), this));
}

void DesktopSessionWin::OnSessionDetached() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(launcher_.get() != NULL);

  launcher_.reset();
}

void DesktopSessionWin::OnDesktopSessionAgentAttached(
      IPC::PlatformFileForTransit desktop_pipe) {
  if (!daemon_process()->OnDesktopSessionAgentAttached(id(),
                                                       desktop_process_,
                                                       desktop_pipe)) {
    RestartDesktopProcess(FROM_HERE);
  }
}

void DesktopSessionWin::OnInjectSas() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Do not try to inject SAS if the desktop process is not running. This can
  // happen when  the session has detached from the console for instance.
  if (!launcher_)
    return;

  if (!sas_injector_)
    sas_injector_ = SasInjector::Create();
  if (!sas_injector_->InjectSas())
    LOG(ERROR) << "Failed to inject Secure Attention Sequence.";
}

void DesktopSessionWin::RestartDesktopProcess(
    const tracked_objects::Location& location) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  launcher_->Send(new ChromotingDaemonDesktopMsg_Crash(
      location.function_name(), location.file_name(), location.line_number()));
}

}  // namespace remoting
