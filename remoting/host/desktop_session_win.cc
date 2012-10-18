// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_session_win.h"

#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_console_monitor.h"
#include "remoting/host/win/wts_session_process_delegate.h"

namespace {

const FilePath::CharType kDesktopBinaryName[] =
    FILE_PATH_LITERAL("remoting_desktop.exe");

// The security descriptor of the daemon IPC endpoint. It gives full access
// to LocalSystem and denies access by anyone else.
const char kDaemonIpcSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

} // namespace

namespace remoting {

DesktopSessionWin::DesktopSessionWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
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
}

bool DesktopSessionWin::OnMessageReceived(const IPC::Message& message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // TODO(alexeypa): Process ChromotingDesktopHostMsg_Initialized messages here.
  // See http://crbug.com/134694.
  return false;
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

  // Construct the host binary name.
  if (desktop_binary_.empty()) {
    FilePath dir_path;
    if (!PathService::Get(base::DIR_EXE, &dir_path)) {
      LOG(ERROR) << "Failed to get the executable file name.";
      OnPermanentError();
      return;
    }
    desktop_binary_ = dir_path.Append(kDesktopBinaryName);
  }

  // Create a delegate to launch a process into the session.
  scoped_ptr<WtsSessionProcessDelegate> delegate(
      new WtsSessionProcessDelegate(main_task_runner_, io_task_runner_,
                                    desktop_binary_, session_id,
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

}  // namespace remoting
