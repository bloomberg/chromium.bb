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
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/host/constants.h"
#include "remoting/host/chromoting_messages.h"
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

const FilePath::CharType kMe2meServiceBinaryName[] =
    FILE_PATH_LITERAL("remoting_service.exe");

// The command line switch specifying the name of the daemon IPC endpoint.
const char kDaemonIpcSwitchName[] = "daemon-pipe";

const char kElevateSwitchName[] = "elevate";

// The command line parameters that should be copied from the service's command
// line to the host process.
const char* kCopiedSwitchNames[] = {
    "host-config", switches::kV, switches::kVModule };

// The security descriptor of the daemon IPC endpoint. It gives full access
// to LocalSystem and denies access by anyone else.
const char kDaemonIpcSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

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
      monitor_(monitor),
      job_state_(kJobUninitialized) {
  monitor_->AddWtsConsoleObserver(this);

  process_exit_event_.Set(CreateEvent(NULL, TRUE, FALSE, NULL));
  CHECK(process_exit_event_.IsValid());

  // To receive job object notifications it is registered with the completion
  // port represented by |ipc_message_loop|. The registration has to be done on
  // the I/O thread because MessageLoopForIO::RegisterJobObject() can only be
  // called via MessageLoopForIO::current().
  ipc_message_loop_->PostTask(FROM_HERE, base::Bind(
      &WtsSessionProcessLauncher::InitializeJob,
      base::Unretained(this)));
}

WtsSessionProcessLauncher::~WtsSessionProcessLauncher() {
  // Make sure that the object is completely stopped. The same check exists
  // in Stoppable::~Stoppable() but this one helps us to fail early and
  // predictably.
  CHECK_EQ(stoppable_state(), Stoppable::kStopped);

  monitor_->RemoveWtsConsoleObserver(this);

  CHECK(!attached_);
  CHECK(!timer_.IsRunning());
}

void WtsSessionProcessLauncher::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());

  // |bytes_transferred| is used in job object notifications to supply
  // the message ID; |context| carries process ID.
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &WtsSessionProcessLauncher::OnJobNotification,
      base::Unretained(this), bytes_transferred,
      reinterpret_cast<DWORD>(context)));
}

bool WtsSessionProcessLauncher::DoLaunchProcess(
    const std::string& channel_name,
    ScopedHandle* process_exit_event_out) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // The job object is not ready. Retry starting the host process later.
  if (!job_.IsValid()) {
    return false;
  }

  // Construct the host binary name.
  FilePath dir_path;
  if (!PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    return false;
  }
  FilePath host_binary = dir_path.Append(kMe2meHostBinaryName);
  FilePath service_binary = dir_path.Append(kMe2meServiceBinaryName);

  // Create the host process command line passing the name of the IPC channel
  // to use and copying known switches from the service's command line.
  CommandLine command_line(service_binary);
  command_line.AppendSwitchPath(kElevateSwitchName, host_binary);
  command_line.AppendSwitchNative(kDaemonIpcSwitchName,
                                  UTF8ToWide(channel_name));
  command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kCopiedSwitchNames,
                                _countof(kCopiedSwitchNames));

  CHECK(ResetEvent(process_exit_event_));

  // Try to launch the process and attach an object watcher to the returned
  // handle so that we get notified when the process terminates.
  base::win::ScopedHandle worker_process;
  base::win::ScopedHandle worker_thread;
  if (!LaunchProcessWithToken(service_binary,
                              command_line.GetCommandLineString(),
                              session_token_,
                              CREATE_SUSPENDED,
                              &worker_process,
                              &worker_thread)) {
    return false;
  }

  if (!AssignProcessToJobObject(job_, worker_process)) {
    LOG_GETLASTERROR(ERROR) << "Failed to assign the worker to the job object";
    TerminateProcess(worker_process, CONTROL_C_EXIT);
    return false;
  }

  if (!ResumeThread(worker_thread)) {
    LOG_GETLASTERROR(ERROR) << "Failed to resume the worker thread";
    DoKillProcess(CONTROL_C_EXIT);
    return false;
  }

  ScopedHandle process_exit_event;
  if (!DuplicateHandle(GetCurrentProcess(),
                       process_exit_event_,
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

  if (job_.IsValid()) {
    TerminateJobObject(job_, exit_code);
  }
}

void WtsSessionProcessLauncher::OnChannelConnected() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
}

bool WtsSessionProcessLauncher::OnMessageReceived(const IPC::Message& message) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  return false;
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

  job_.Close();

  // Drain the completion queue to make sure all job object notification have
  // been received.
  if (job_state_ == kJobRunning) {
    job_state_ = kJobStopping;
    ipc_message_loop_->PostTask(FROM_HERE, base::Bind(
        &WtsSessionProcessLauncher::DrainJobNotifications,
        base::Unretained(this)));
  }

  // Don't complete shutdown if |launcher_| is not completely stopped.
  if (launcher_.get() != NULL) {
    return;
  }

  // Don't complete shutdown if the completion queue hasn't been drained.
  if (job_state_ != kJobUninitialized && job_state_ != kJobStopped) {
    return;
  }

  CompleteStopping();
}

void WtsSessionProcessLauncher::DrainJobNotifications() {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());

  // DrainJobNotifications() is posted after the job object is destroyed, so
  // by this time all notifications from the job object have been processed
  // already. Let the main thread know that the queue has been drained.
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &WtsSessionProcessLauncher::DrainJobNotificationsCompleted,
      base::Unretained(this)));
}

void WtsSessionProcessLauncher::DrainJobNotificationsCompleted() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(job_state_, kJobStopping);

  job_state_ = kJobStopped;
  Stop();
}

void WtsSessionProcessLauncher::InitializeJob() {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());

  ScopedHandle job;
  job.Set(CreateJobObject(NULL, NULL));
  if (!job.IsValid()) {
    LOG_GETLASTERROR(ERROR) << "Failed to create a job object";
    return;
  }

  // Limit the number of active processes in the job to two (the process
  // performing elevation and the host) and make sure that all processes will be
  // killed once the job object is destroyed.
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
  memset(&info, 0, sizeof(info));
  info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
      JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  info.BasicLimitInformation.ActiveProcessLimit = 2;
  if (!SetInformationJobObject(job,
                               JobObjectExtendedLimitInformation,
                               &info,
                               sizeof(info))) {
    LOG_GETLASTERROR(ERROR) << "Failed to set limits on the job object";
    return;
  }

  // Register the job object with the completion port in the I/O thread to
  // receive job notifications.
  if (!MessageLoopForIO::current()->RegisterJobObject(job, this)) {
    LOG_GETLASTERROR(ERROR)
        << "Failed to associate the job object with a completion port";
    return;
  }

  // ScopedHandle is not compatible with base::Passed, so we wrap it to a scoped
  // pointer.
  scoped_ptr<ScopedHandle> job_wrapper(new ScopedHandle());
  *job_wrapper = job.Pass();

  // Let the main thread know that initialization is complete.
  main_message_loop_->PostTask(FROM_HERE, base::Bind(
      &WtsSessionProcessLauncher::InitializeJobCompleted,
      base::Unretained(this), base::Passed(&job_wrapper)));
}

void WtsSessionProcessLauncher::InitializeJobCompleted(
    scoped_ptr<ScopedHandle> job) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(!job_.IsValid());
  DCHECK_EQ(job_state_, kJobUninitialized);

  if (stoppable_state() == Stoppable::kRunning) {
    job_ = job->Pass();
    job_state_ = kJobRunning;
  }
}

void WtsSessionProcessLauncher::OnJobNotification(DWORD message, DWORD pid) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  switch (message) {
    case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:
      CHECK(SetEvent(process_exit_event_));
      break;

    case JOB_OBJECT_MSG_NEW_PROCESS:
      // We report the exit code of the worker process to be |CONTROL_C_EXIT|
      // if we cannot get the actual exit code. So here we can safely ignore
      // the error returned by OpenProcess().
      worker_process_.Set(OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid));
      break;
  }
}

void WtsSessionProcessLauncher::LaunchProcess() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  DCHECK(attached_);
  DCHECK(launcher_.get() == NULL);
  DCHECK(!timer_.IsRunning());

  launch_time_ = base::Time::Now();
  launcher_.reset(new WorkerProcessLauncher(
      this, this,
      base::Bind(&WtsSessionProcessLauncher::OnLauncherStopped,
                 base::Unretained(this)),
      main_message_loop_,
      ipc_message_loop_));
  launcher_->Start(kDaemonIpcSecurityDescriptor);
}

void WtsSessionProcessLauncher::OnLauncherStopped() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  DWORD exit_code = CONTROL_C_EXIT;
  if (worker_process_.IsValid()) {
    if (!::GetExitCodeProcess(worker_process_, &exit_code)) {
      LOG_GETLASTERROR(INFO)
          << "Failed to query the exit code of the worker process";
      exit_code = CONTROL_C_EXIT;
    }

    worker_process_.Close();
  }

  launcher_.reset(NULL);

  // Do not relaunch the worker process if the caller has asked us to stop.
  if (stoppable_state() != Stoppable::kRunning) {
    Stop();
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

} // namespace remoting
