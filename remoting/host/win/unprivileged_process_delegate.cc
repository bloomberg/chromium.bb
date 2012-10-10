// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/unprivileged_process_delegate.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/win/launch_process_with_token.h"

using base::win::ScopedHandle;

namespace {

// The command line switch specifying the name of the daemon IPC endpoint.
const char kDaemonIpcSwitchName[] = "daemon-pipe";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = {
    "host-config", switches::kV, switches::kVModule };

} // namespace

namespace remoting {

UnprivilegedProcessDelegate::UnprivilegedProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const FilePath& binary_path)
    : main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner),
      binary_path_(binary_path) {
}

UnprivilegedProcessDelegate::~UnprivilegedProcessDelegate() {
  KillProcess(CONTROL_C_EXIT);
}

DWORD UnprivilegedProcessDelegate::GetExitCode() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  DWORD exit_code = CONTROL_C_EXIT;
  if (worker_process_.IsValid()) {
    if (!::GetExitCodeProcess(worker_process_, &exit_code)) {
      LOG_GETLASTERROR(INFO)
          << "Failed to query the exit code of the worker process";
      exit_code = CONTROL_C_EXIT;
    }
  }

  return exit_code;
}

void UnprivilegedProcessDelegate::KillProcess(DWORD exit_code) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (worker_process_.IsValid()) {
    TerminateProcess(worker_process_, exit_code);
  }
}

bool UnprivilegedProcessDelegate::LaunchProcess(
    const std::string& channel_name,
    ScopedHandle* process_exit_event_out) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  // Create the command line passing the name of the IPC channel to use and
  // copying known switches from the caller's command line.
  CommandLine command_line(binary_path_);
  command_line.AppendSwitchNative(kDaemonIpcSwitchName,
                                  UTF8ToWide(channel_name));
  command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kCopiedSwitchNames,
                                arraysize(kCopiedSwitchNames));

  // Try to launch the process.
  // TODO(alexeypa): Pass a restricted process token.
  // See http://crbug.com/134694.
  ScopedHandle worker_thread;
  worker_process_.Close();
  if (!LaunchProcessWithToken(command_line.GetProgram(),
                              command_line.GetCommandLineString(),
                              NULL,
                              0,
                              &worker_process_,
                              &worker_thread)) {
    return false;
  }

  // Return a handle that the caller can wait on to get notified when
  // the process terminates.
  ScopedHandle process_exit_event;
  if (!DuplicateHandle(GetCurrentProcess(),
                       worker_process_,
                       GetCurrentProcess(),
                       process_exit_event.Receive(),
                       SYNCHRONIZE,
                       FALSE,
                       0)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate a handle";
    KillProcess(CONTROL_C_EXIT);
    return false;
  }

  *process_exit_event_out = process_exit_event.Pass();
  return true;
}

} // namespace remoting
