// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "remoting/host/constants.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/worker_process_launcher.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// The minimum and maximum delays between attempts to launch the networking
// process.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

const FilePath::CharType kMe2meHostBinaryName[] =
    FILE_PATH_LITERAL("remoting_me2me_host.exe");

// The IPC channel name is passed to the networking process in the command line.
const char kDaemonPipeSwitchName[] = "daemon-pipe";

// The command line parameters that should be copied from the service's command
// line to the network process.
const char* kCopiedSwitchNames[] = {
    "host-config", switches::kV, switches::kVModule };

// The security descriptor of the daemon IPC endpoint. It gives full access
// to LocalSystem and denies access by anyone else.
const char kDaemonPipeSecurityDescriptor[] = "O:SYG:SYD:(A;;GA;;;SY)";

} // namespace

namespace remoting {

class DaemonProcessWin : public DaemonProcess,
                         public WorkerProcessLauncher::Delegate {
 public:
  DaemonProcessWin(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                   const base::Closure& stopped_callback);
  virtual ~DaemonProcessWin();

  virtual void OnChannelConnected() OVERRIDE;

  // Sends an IPC message to the worker process. This method can be called only
  // after successful Start() and until Stop() is called or an error occurred.
  virtual void Send(IPC::Message* message) OVERRIDE;

  // WorkerProcessLauncher::Delegate implementation.
  virtual bool DoLaunchProcess(
      const std::string& channel_name,
      ScopedHandle* process_exit_event_out) OVERRIDE;
  virtual void DoKillProcess(DWORD exit_code) OVERRIDE;

 protected:
  // Stoppable implementation.
  virtual void DoStop() OVERRIDE;

  // DaemonProcess implementation.
  virtual void LaunchNetworkProcess() OVERRIDE;

 private:
  // Called when the launcher reports the worker process has stopped.
  void OnLauncherStopped();

  // True if the network process is connected to the daemon.
  bool connected_;

  // Time of the last launch attempt.
  base::Time launch_time_;

  // Current backoff delay.
  base::TimeDelta launch_backoff_;

  // Timer used to schedule the next attempt to launch the process.
  base::OneShotTimer<DaemonProcessWin> timer_;

  scoped_ptr<WorkerProcessLauncher> launcher_;

  ScopedHandle network_process_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcessWin);
};

DaemonProcessWin::DaemonProcessWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : DaemonProcess(main_task_runner, io_task_runner, stopped_callback),
      connected_(false) {
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
  DCHECK(!network_process_.IsValid());
  DCHECK(!timer_.IsRunning());

  launch_time_ = base::Time::Now();
  launcher_.reset(new WorkerProcessLauncher(
      this, this,
      base::Bind(&DaemonProcessWin::OnLauncherStopped, base::Unretained(this)),
      main_task_runner(),
      io_task_runner()));
  launcher_->Start(kDaemonPipeSecurityDescriptor);
}

void DaemonProcessWin::OnChannelConnected() {
  connected_ = true;
  DaemonProcess::OnChannelConnected();
}

void DaemonProcessWin::Send(IPC::Message* message) {
  if (connected_) {
    launcher_->Send(message);
  } else {
    delete message;
  }
}

bool DaemonProcessWin::DoLaunchProcess(
    const std::string& channel_name,
    ScopedHandle* process_exit_event_out) {
  DCHECK(main_task_runner()->BelongsToCurrentThread());
  DCHECK(!network_process_.IsValid());

  // Construct the host binary name.
  FilePath dir_path;
  if (!PathService::Get(base::DIR_EXE, &dir_path)) {
    LOG(ERROR) << "Failed to get the executable file name.";
    return false;
  }
  FilePath host_binary = dir_path.Append(kMe2meHostBinaryName);

  // Create the host process command line, passing the name of the IPC channel
  // to use and copying known switches from the service's command line.
  CommandLine command_line(host_binary);
  command_line.AppendSwitchNative(kDaemonPipeSwitchName,
                                  UTF8ToWide(channel_name));
  command_line.CopySwitchesFrom(*CommandLine::ForCurrentProcess(),
                                kCopiedSwitchNames,
                                _countof(kCopiedSwitchNames));

  ScopedHandle token;
  if (!OpenProcessToken(GetCurrentProcess(),
                        MAXIMUM_ALLOWED,
                        token.Receive())) {
    LOG_GETLASTERROR(FATAL) << "Failed to open process token";
    return false;
  }

  // Try to launch the process and attach an object watcher to the returned
  // handle so that we get notified when the process terminates.
  // TODO(alexeypa): Pass a restricted process token.
  // See http://crbug.com/134694.
  ScopedHandle worker_thread;
  if (!LaunchProcessWithToken(host_binary,
                              command_line.GetCommandLineString(),
                              token,
                              0,
                              &network_process_,
                              &worker_thread)) {
    return false;
  }

  ScopedHandle process_exit_event;
  if (!DuplicateHandle(GetCurrentProcess(),
                       network_process_,
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

void DaemonProcessWin::DoKillProcess(DWORD exit_code) {
  DCHECK(main_task_runner()->BelongsToCurrentThread());
  CHECK(network_process_.IsValid());

  TerminateProcess(network_process_, exit_code);
}

void DaemonProcessWin::DoStop() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());

  timer_.Stop();

  if (launcher_.get() != NULL) {
    launcher_->Stop();
  }

  // Early exit if we're still waiting for |launcher_| to stop.
  if (launcher_.get() != NULL) {
    return;
  }

  DaemonProcess::DoStop();
}

void DaemonProcessWin::OnLauncherStopped() {
  DCHECK(main_task_runner()->BelongsToCurrentThread());
  CHECK(network_process_.IsValid());

  DWORD exit_code = CONTROL_C_EXIT;
  if (!::GetExitCodeProcess(network_process_, &exit_code)) {
    LOG_GETLASTERROR(INFO)
        << "Failed to query the exit code of the worker process";
    exit_code = CONTROL_C_EXIT;
  }

  network_process_.Close();
  connected_ = false;
  launcher_.reset(NULL);

  // Do not relaunch the network process if the caller has asked us to stop.
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
               this, &DaemonProcessWin::LaunchNetworkProcess);
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
