// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/wts_session_process_launcher.h"

#include <windows.h>
#include <sddl.h>
#include <limits>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/constants.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/sas_injector.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/wts_console_monitor.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// The minimum and maximum delays between attempts to inject host process into
// a session.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

const FilePath::CharType kMe2meHostBinaryName[] =
    FILE_PATH_LITERAL("remoting_me2me_host.exe");

// The IPC channel name is passed to the host in the command line.
const char kChromotingIpcSwitchName[] = "chromoting-ipc";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = {
    "auth-config", "host-config", switches::kV, switches::kVModule };

// The security descriptor of the Chromoting IPC channel. It gives full access
// to LocalSystem and denies access by anyone else.
const char kChromotingChannelSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

} // namespace

namespace remoting {

WtsSessionProcessLauncher::WtsSessionProcessLauncher(
    const base::Closure& stopped_callback,
    WtsConsoleMonitor* monitor,
    scoped_refptr<base::SingleThreadTaskRunner> main_message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_message_loop)
    : Stoppable(main_message_loop, stopped_callback),
      attached_(false),
      main_message_loop_(main_message_loop),
      ipc_message_loop_(ipc_message_loop),
      monitor_(monitor) {
  monitor_->AddWtsConsoleObserver(this);
}

WtsSessionProcessLauncher::~WtsSessionProcessLauncher() {
  monitor_->RemoveWtsConsoleObserver(this);

  DCHECK(!attached_);
  DCHECK(!timer_.IsRunning());
}

void WtsSessionProcessLauncher::LaunchProcess() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(attached_);
  DCHECK(launcher_.get() == NULL);
  DCHECK(!timer_.IsRunning());
  DCHECK(!worker_process_.IsValid());

  launch_time_ = base::Time::Now();
  launcher_.reset(new WorkerProcessLauncher(
      this,
      base::Bind(&WtsSessionProcessLauncher::OnLauncherStopped,
                 base::Unretained(this)),
      main_message_loop_,
      ipc_message_loop_));
  launcher_->Start(kChromotingChannelSecurityDescriptor);
}

void WtsSessionProcessLauncher::OnLauncherStopped() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  DWORD exit_code;
  if (!::GetExitCodeProcess(worker_process_, &exit_code)) {
    LOG_GETLASTERROR(INFO)
        << "Failed to query the exit code of the worker process";
    exit_code = CONTROL_C_EXIT;
  }

  launcher_.reset(NULL);
  worker_process_.Close();

  // Do not relaunch the worker process if the caller has asked us to stop.
  if (stoppable_state() != Stoppable::kRunning) {
    CompleteStopping();
    return;
  }

  // Stop trying to restart the worker process if its process exited due to
  // misconfiguration.
  if (kMinPermanentErrorExitCode <= exit_code &&
      exit_code <= kMaxPermanentErrorExitCode) {
    Stop();
    return;
  }

  // Try to restart the worker process if we are still attached to a session.
  if (attached_) {
    // Expand the backoff interval if the process has died quickly or reset it
    // if it was up longer than the maximum backoff delay.
    base::TimeDelta delta = base::Time::Now() - launch_time_;
    if (delta < base::TimeDelta() ||
        delta >= base::TimeDelta::FromSeconds(kMaxLaunchDelaySeconds)) {
      launch_backoff_ = base::TimeDelta();
    } else {
      launch_backoff_ = std::max(
          launch_backoff_ * 2, TimeDelta::FromSeconds(kMinLaunchDelaySeconds));
      launch_backoff_ = std::min(
          launch_backoff_, TimeDelta::FromSeconds(kMaxLaunchDelaySeconds));
    }

    // Try to launch the worker process.
    timer_.Start(FROM_HERE, launch_backoff_,
                 this, &WtsSessionProcessLauncher::LaunchProcess);
  }
}

bool WtsSessionProcessLauncher::DoLaunchProcess(
    const std::string& channel_name,
    ScopedHandle* process_exit_event_out) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(!worker_process_.IsValid());

  // Construct the host binary name.
  FilePath dir_path;
  if (!PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    return false;
  }
  FilePath host_binary = dir_path.Append(kMe2meHostBinaryName);

  // Create the host process command line passing the name of the IPC channel
  // to use and copying known switches from the service's command line.
  CommandLine command_line(host_binary);
  command_line.AppendSwitchNative(kChromotingIpcSwitchName,
                                  UTF8ToWide(channel_name));
  command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kCopiedSwitchNames,
                                _countof(kCopiedSwitchNames));

  // Try to launch the process and attach an object watcher to the returned
  // handle so that we get notified when the process terminates.
  if (!LaunchProcessWithToken(host_binary,
                              command_line.GetCommandLineString(),
                              session_token_,
                              &worker_process_)) {
    return false;
  }

  ScopedHandle process_exit_event;
  if (!DuplicateHandle(GetCurrentProcess(),
                       worker_process_,
                       GetCurrentProcess(),
                       process_exit_event.Receive(),
                       SYNCHRONIZE,
                       FALSE,
                       0)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate a handle";
    DoKillProcess(CONTROL_C_EXIT);
    return false;
  }

  *process_exit_event_out = process_exit_event.Pass();
  return true;
}

void WtsSessionProcessLauncher::DoKillProcess(DWORD exit_code) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  if (worker_process_.IsValid()) {
    TerminateProcess(worker_process_, exit_code);
  }
}

void WtsSessionProcessLauncher::OnChannelConnected() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
}

bool WtsSessionProcessLauncher::OnMessageReceived(const IPC::Message& message) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WtsSessionProcessLauncher, message)
      IPC_MESSAGE_HANDLER(ChromotingHostMsg_SendSasToConsole,
                          OnSendSasToConsole)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WtsSessionProcessLauncher::OnSendSasToConsole() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  if (attached_) {
    if (sas_injector_.get() == NULL) {
      sas_injector_ = SasInjector::Create();
    }

    if (sas_injector_.get() != NULL) {
      sas_injector_->InjectSas();
    }
  }
}

void WtsSessionProcessLauncher::OnSessionAttached(uint32 session_id) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  if (stoppable_state() != Stoppable::kRunning) {
    return;
  }

  DCHECK(!attached_);
  DCHECK(!timer_.IsRunning());

  attached_ = true;

  // Create a session token for the launched process.
  if (!CreateSessionToken(session_id, &session_token_))
    return;

  // Now try to launch the host.
  LaunchProcess();
}

void WtsSessionProcessLauncher::OnSessionDetached() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(attached_);

  attached_ = false;
  launch_backoff_ = base::TimeDelta();
  session_token_.Close();
  timer_.Stop();

  if (launcher_.get() != NULL) {
    launcher_->Stop();
  }
}

void WtsSessionProcessLauncher::DoStop() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  if (attached_) {
    OnSessionDetached();
  }

  if (launcher_.get() == NULL) {
    CompleteStopping();
  }
}

} // namespace remoting
