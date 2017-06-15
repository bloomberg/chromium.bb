// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/host/service_process_launcher.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/system/core.h"
#include "services/service_manager/public/cpp/standalone_service/switches.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"

#if defined(OS_LINUX)
#include "sandbox/linux/services/namespace_sandbox.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_MACOSX)
#include "services/service_manager/public/cpp/standalone_service/mach_broker.h"
#endif

namespace service_manager {

ServiceProcessLauncher::ServiceProcessLauncher(
    base::TaskRunner* launch_process_runner,
    ServiceProcessLauncherDelegate* delegate,
    const base::FilePath& service_path)
    : launch_process_runner_(launch_process_runner),
      delegate_(delegate),
      start_sandboxed_(false),
      service_path_(service_path),
      start_child_process_event_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      weak_factory_(this) {
  if (service_path_.empty())
    service_path_ = base::CommandLine::ForCurrentProcess()->GetProgram();
}

ServiceProcessLauncher::~ServiceProcessLauncher() {
  Join();
}

mojom::ServicePtr ServiceProcessLauncher::Start(
    const Identity& target,
    bool start_sandboxed,
    const ProcessReadyCallback& callback) {
  DCHECK(!child_process_.IsValid());

  start_sandboxed_ = start_sandboxed;
  target_ = target;

  const base::CommandLine& parent_command_line =
      *base::CommandLine::ForCurrentProcess();

  std::unique_ptr<base::CommandLine> child_command_line(
      new base::CommandLine(service_path_));

  child_command_line->AppendArguments(parent_command_line, false);
  child_command_line->AppendSwitchASCII(switches::kServiceName, target.name());
#ifndef NDEBUG
  child_command_line->AppendSwitchASCII("u", target.user_id());
#endif

  if (start_sandboxed_)
    child_command_line->AppendSwitch(switches::kEnableSandbox);

  mojo_ipc_channel_.reset(new mojo::edk::PlatformChannelPair);
  mojo_ipc_channel_->PrepareToPassClientHandleToChildProcess(
      child_command_line.get(), &handle_passing_info_);

  mojom::ServicePtr client = PassServiceRequestOnCommandLine(
      &broker_client_invitation_, child_command_line.get());

  launch_process_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&ServiceProcessLauncher::DoLaunch, base::Unretained(this),
                 base::Passed(&child_command_line)),
      base::Bind(&ServiceProcessLauncher::DidStart,
                 weak_factory_.GetWeakPtr(), callback));
  return client;
}

void ServiceProcessLauncher::Join() {
  if (mojo_ipc_channel_)
    start_child_process_event_.Wait();
  mojo_ipc_channel_.reset();
  if (child_process_.IsValid()) {
    int rv = -1;
    LOG_IF(ERROR, !child_process_.WaitForExit(&rv))
        << "Failed to wait for child process";
    child_process_.Close();
  }
}

void ServiceProcessLauncher::DidStart(const ProcessReadyCallback& callback) {
  if (child_process_.IsValid()) {
    callback.Run(child_process_.Pid());
  } else {
    LOG(ERROR) << "Failed to start child process";
    mojo_ipc_channel_.reset();
    callback.Run(base::kNullProcessId);
  }
}

void ServiceProcessLauncher::DoLaunch(
    std::unique_ptr<base::CommandLine> child_command_line) {
  if (delegate_) {
    delegate_->AdjustCommandLineArgumentsForTarget(target_,
                                                   child_command_line.get());
  }

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.handles_to_inherit = &handle_passing_info_;
#if defined(OFFICIAL_BUILD)
  CHECK(false) << "Launching mojo process with inherit_handles is insecure!";
#endif
  options.inherit_handles = true;
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  options.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  // Always inherit stdout/stderr as a pair.
  if (!options.stdout_handle || !options.stdin_handle)
    options.stdin_handle = options.stdout_handle = nullptr;

  // Pseudo handles are used when stdout and stderr redirect to the console. In
  // that case, they're automatically inherited by child processes. See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682075.aspx
  // Trying to add them to the list of handles to inherit causes CreateProcess
  // to fail. When this process is launched from Python then a real handle is
  // used. In that case, we do want to add it to the list of handles that is
  // inherited.
  if (options.stdout_handle &&
      GetFileType(options.stdout_handle) != FILE_TYPE_CHAR) {
    handle_passing_info_.push_back(options.stdout_handle);
  }
  if (options.stderr_handle &&
      GetFileType(options.stderr_handle) != FILE_TYPE_CHAR &&
      options.stdout_handle != options.stderr_handle) {
    handle_passing_info_.push_back(options.stderr_handle);
  }
#elif defined(OS_POSIX)
  handle_passing_info_.push_back(std::make_pair(STDIN_FILENO, STDIN_FILENO));
  handle_passing_info_.push_back(std::make_pair(STDOUT_FILENO, STDOUT_FILENO));
  handle_passing_info_.push_back(std::make_pair(STDERR_FILENO, STDERR_FILENO));
  options.fds_to_remap = &handle_passing_info_;
#endif
  DVLOG(2) << "Launching child with command line: "
           << child_command_line->GetCommandLineString();
#if defined(OS_LINUX)
  if (start_sandboxed_) {
    child_process_ =
        sandbox::NamespaceSandbox::LaunchProcess(*child_command_line, options);
    if (!child_process_.IsValid()) {
      LOG(ERROR) << "Starting the process with a sandbox failed. Missing kernel"
                 << " support.";
    }
  } else
#endif
  {
#if defined(OS_MACOSX)
    MachBroker* mach_broker = MachBroker::GetInstance();
    base::AutoLock locker(mach_broker->GetLock());
#endif
    child_process_ = base::LaunchProcess(*child_command_line, options);
#if defined(OS_MACOSX)
    mach_broker->ExpectPid(child_process_.Handle());
#endif
  }

  if (child_process_.IsValid()) {
#if defined(OS_CHROMEOS)
    // Always log instead of DVLOG because knowing which pid maps to which
    // service is vital for interpreting crashes after-the-fact and Chrome OS
    // devices generally run release builds, even in development.
    VLOG(0)
#else
    DVLOG(0)
#endif
        << "Launched child process pid=" << child_process_.Pid()
        << ", instance=" << target_.instance() << ", name=" << target_.name()
        << ", user_id=" << target_.user_id();

    if (mojo_ipc_channel_.get()) {
      mojo_ipc_channel_->ChildProcessLaunched();
      broker_client_invitation_.Send(
          child_process_.Handle(),
          mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                      mojo_ipc_channel_->PassServerHandle()));
    }
  }
  start_child_process_event_.Signal();
}

}  // namespace service_manager
