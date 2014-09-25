// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/wts_session_process_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/ipc_util.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_terminal_monitor.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::win::ScopedHandle;

// Name of the default session desktop.
const char kDefaultDesktopName[] = "winsta0\\default";

namespace remoting {

// A private class actually implementing the functionality provided by
// |WtsSessionProcessDelegate|. This class is ref-counted and implements
// asynchronous fire-and-forget shutdown.
class WtsSessionProcessDelegate::Core
    : public base::RefCountedThreadSafe<Core>,
      public base::MessagePumpForIO::IOHandler,
      public IPC::Listener {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
       scoped_ptr<CommandLine> target,
       bool launch_elevated,
       const std::string& channel_security);

  // Initializes the object returning true on success.
  bool Initialize(uint32 session_id);

  // Stops the object asynchronously.
  void Stop();

  // Mirrors WorkerProcessLauncher::Delegate.
  void LaunchProcess(WorkerProcessLauncher* event_handler);
  void Send(IPC::Message* message);
  void CloseChannel();
  void KillProcess();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // base::MessagePumpForIO::IOHandler implementation.
  virtual void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                             DWORD bytes_transferred,
                             DWORD error) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // The actual implementation of LaunchProcess()
  void DoLaunchProcess();

  // Drains the completion port queue to make sure that all job object
  // notifications have been received.
  void DrainJobNotifications();

  // Notified that the completion port queue has been drained.
  void DrainJobNotificationsCompleted();

  // Creates and initializes the job object that will sandbox the launched child
  // processes.
  void InitializeJob(scoped_ptr<base::win::ScopedHandle> job);

  // Notified that the job object initialization is complete.
  void InitializeJobCompleted(scoped_ptr<base::win::ScopedHandle> job);

  // Called when the number of processes running in the job reaches zero.
  void OnActiveProcessZero();

  void ReportFatalError();
  void ReportProcessLaunched(base::win::ScopedHandle worker_process);

  // The task runner all public methods of this class should be called on.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  scoped_ptr<IPC::ChannelProxy> channel_;

  // Security descriptor (as SDDL) to be applied to |channel_|.
  std::string channel_security_;

  WorkerProcessLauncher* event_handler_;

  // Pointer to GetNamedPipeClientProcessId() API if it is available.
  typedef BOOL (WINAPI * GetNamedPipeClientProcessIdFn)(HANDLE, DWORD*);
  GetNamedPipeClientProcessIdFn get_named_pipe_client_pid_;

  // The job object used to control the lifetime of child processes.
  base::win::ScopedHandle job_;

  // True if the worker process should be launched elevated.
  bool launch_elevated_;

  // True if a laucnh attemp is pending.
  bool launch_pending_;

  // The named pipe used as the transport by |channel_|.
  base::win::ScopedHandle pipe_;

  // The token to be used to launch a process in a different session.
  base::win::ScopedHandle session_token_;

  // Command line of the launched process.
  scoped_ptr<CommandLine> target_command_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

WtsSessionProcessDelegate::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<CommandLine> target_command,
    bool launch_elevated,
    const std::string& channel_security)
    : caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      channel_security_(channel_security),
      event_handler_(NULL),
      get_named_pipe_client_pid_(NULL),
      launch_elevated_(launch_elevated),
      launch_pending_(false),
      target_command_(target_command.Pass()) {
}

bool WtsSessionProcessDelegate::Core::Initialize(uint32 session_id) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Windows XP does not support elevation.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    launch_elevated_ = false;

  if (launch_elevated_) {
    // GetNamedPipeClientProcessId() is available starting from Vista.
    HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
    CHECK(kernel32 != NULL);

    get_named_pipe_client_pid_ =
        reinterpret_cast<GetNamedPipeClientProcessIdFn>(
            GetProcAddress(kernel32, "GetNamedPipeClientProcessId"));
    CHECK(get_named_pipe_client_pid_ != NULL);

    ScopedHandle job;
    job.Set(CreateJobObject(NULL, NULL));
    if (!job.IsValid()) {
      PLOG(ERROR) << "Failed to create a job object";
      return false;
    }

    // Limit the number of active processes in the job to two (the helper
    // process performing elevation and the worker process itself) and make sure
    // that all processes will be killed once the job object is destroyed.
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    memset(&info, 0, sizeof(info));
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS |
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    info.BasicLimitInformation.ActiveProcessLimit = 2;
    if (!SetInformationJobObject(job.Get(),
                                 JobObjectExtendedLimitInformation,
                                 &info,
                                 sizeof(info))) {
      PLOG(ERROR) << "Failed to set limits on the job object";
      return false;
    }

    // ScopedHandle is not compatible with base::Passed, so we wrap it to
    // a scoped pointer.
    scoped_ptr<ScopedHandle> job_wrapper(new ScopedHandle());
    *job_wrapper = job.Pass();

    // To receive job object notifications the job object is registered with
    // the completion port represented by |io_task_runner|. The registration has
    // to be done on the I/O thread because
    // MessageLoopForIO::RegisterJobObject() can only be called via
    // MessageLoopForIO::current().
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Core::InitializeJob, this, base::Passed(&job_wrapper)));
  }

  // Create a session token for the launched process.
  return CreateSessionToken(session_id, &session_token_);
}

void WtsSessionProcessDelegate::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  KillProcess();

  // Drain the completion queue to make sure all job object notifications have
  // been received.
  DrainJobNotificationsCompleted();
}

void WtsSessionProcessDelegate::Core::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!event_handler_);

  event_handler_ = event_handler;
  DoLaunchProcess();
}

void WtsSessionProcessDelegate::Core::Send(IPC::Message* message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (channel_) {
    channel_->Send(message);
  } else {
    delete message;
  }
}

void WtsSessionProcessDelegate::Core::CloseChannel() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  channel_.reset();
  pipe_.Close();
}

void WtsSessionProcessDelegate::Core::KillProcess() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  channel_.reset();
  event_handler_ = NULL;
  launch_pending_ = false;
  pipe_.Close();

  if (launch_elevated_) {
    if (job_.IsValid())
      TerminateJobObject(job_.Get(), CONTROL_C_EXIT);
  } else {
    if (worker_process_.IsValid())
      TerminateProcess(worker_process_.Get(), CONTROL_C_EXIT);
  }

  worker_process_.Close();
}

WtsSessionProcessDelegate::Core::~Core() {
  DCHECK(!channel_);
  DCHECK(!event_handler_);
  DCHECK(!pipe_.IsValid());
  DCHECK(!worker_process_.IsValid());
}

void WtsSessionProcessDelegate::Core::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // |bytes_transferred| is used in job object notifications to supply
  // the message ID; |context| carries process ID.
  if (bytes_transferred == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
    caller_task_runner_->PostTask(FROM_HERE,
                                  base::Bind(&Core::OnActiveProcessZero, this));
  }
}

bool WtsSessionProcessDelegate::Core::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  return event_handler_->OnMessageReceived(message);
}

void WtsSessionProcessDelegate::Core::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Report the worker PID now if the worker process is launched indirectly.
  // Note that in this case the pipe's security descriptor is the only
  // protection against a malicious processed connecting to the pipe.
  if (launch_elevated_) {
    DWORD pid;
    if (!get_named_pipe_client_pid_(pipe_.Get(), &pid)) {
      PLOG(ERROR) << "Failed to retrive PID of the client";
      ReportFatalError();
      return;
    }

    if (pid != static_cast<DWORD>(peer_pid)) {
      LOG(ERROR) << "The actual client PID " << pid
                 << " does not match the one reported by the client: "
                 << peer_pid;
      ReportFatalError();
      return;
    }

    DWORD desired_access =
        SYNCHRONIZE | PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION;
    ScopedHandle worker_process(OpenProcess(desired_access, false, pid));
    if (!worker_process.IsValid()) {
      PLOG(ERROR) << "Failed to open process " << pid;
      ReportFatalError();
      return;
    }

    ReportProcessLaunched(worker_process.Pass());
  }

  if (event_handler_)
    event_handler_->OnChannelConnected(peer_pid);
}

void WtsSessionProcessDelegate::Core::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  event_handler_->OnChannelError();
}

void WtsSessionProcessDelegate::Core::DoLaunchProcess() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!channel_);
  DCHECK(!pipe_.IsValid());
  DCHECK(!worker_process_.IsValid());

  base::CommandLine command_line(target_command_->argv());
  if (launch_elevated_) {
    // The job object is not ready. Retry starting the host process later.
    if (!job_.IsValid()) {
      launch_pending_ = true;
      return;
    }

    // Construct the helper binary name.
    base::FilePath helper_binary;
    if (!GetInstalledBinaryPath(kHostBinaryName, &helper_binary)) {
      ReportFatalError();
      return;
    }

    // Create the command line passing the name of the IPC channel to use and
    // copying known switches from the caller's command line.
    command_line.SetProgram(helper_binary);
    command_line.AppendSwitchPath(kElevateSwitchName,
                                  target_command_->GetProgram());
  }

  // Create the server end of the IPC channel.
  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();
  ScopedHandle pipe;
  if (!CreateIpcChannel(channel_name, channel_security_, &pipe)) {
    ReportFatalError();
    return;
  }

  // Wrap the pipe into an IPC channel.
  scoped_ptr<IPC::ChannelProxy> channel(
      IPC::ChannelProxy::Create(IPC::ChannelHandle(pipe.Get()),
                                IPC::Channel::MODE_SERVER,
                                this,
                                io_task_runner_));

  // Pass the name of the IPC channel to use.
  command_line.AppendSwitchNative(kDaemonPipeSwitchName,
                                  base::UTF8ToWide(channel_name));

  // Try to launch the process.
  ScopedHandle worker_process;
  ScopedHandle worker_thread;
  if (!LaunchProcessWithToken(command_line.GetProgram(),
                              command_line.GetCommandLineString(),
                              session_token_.Get(),
                              NULL,
                              NULL,
                              false,
                              CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
                              base::UTF8ToUTF16(kDefaultDesktopName).c_str(),
                              &worker_process,
                              &worker_thread)) {
    ReportFatalError();
    return;
  }

  if (launch_elevated_) {
    if (!AssignProcessToJobObject(job_.Get(), worker_process.Get())) {
      PLOG(ERROR) << "Failed to assign the worker to the job object";
      ReportFatalError();
      return;
    }
  }

  if (!ResumeThread(worker_thread.Get())) {
    PLOG(ERROR) << "Failed to resume the worker thread";
    ReportFatalError();
    return;
  }

  channel_ = channel.Pass();
  pipe_ = pipe.Pass();

  // Report success if the worker process is lauched directly. Otherwise, PID of
  // the client connected to the pipe will be used later. See
  // OnChannelConnected().
  if (!launch_elevated_)
    ReportProcessLaunched(worker_process.Pass());
}

void WtsSessionProcessDelegate::Core::DrainJobNotifications() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // DrainJobNotifications() is posted after the job object is destroyed, so
  // by this time all notifications from the job object have been processed
  // already. Let the main thread know that the queue has been drained.
  caller_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Core::DrainJobNotificationsCompleted, this));
}

void WtsSessionProcessDelegate::Core::DrainJobNotificationsCompleted() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (job_.IsValid()) {
    job_.Close();

    // Drain the completion queue to make sure all job object notification have
    // been received.
    io_task_runner_->PostTask(FROM_HERE, base::Bind(
        &Core::DrainJobNotifications, this));
  }
}

void WtsSessionProcessDelegate::Core::InitializeJob(
    scoped_ptr<base::win::ScopedHandle> job) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Register to receive job notifications via the I/O thread's completion port.
  if (!base::MessageLoopForIO::current()->RegisterJobObject(job->Get(), this)) {
    PLOG(ERROR) << "Failed to associate the job object with a completion port";
    return;
  }

  // Let the main thread know that initialization is complete.
  caller_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Core::InitializeJobCompleted, this, base::Passed(&job)));
}

void WtsSessionProcessDelegate::Core::InitializeJobCompleted(
    scoped_ptr<ScopedHandle> job) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!job_.IsValid());

  job_ = job->Pass();

  if (launch_pending_)
    DoLaunchProcess();
}

void WtsSessionProcessDelegate::Core::OnActiveProcessZero() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (launch_pending_) {
    LOG(ERROR) << "The worker process exited before connecting via IPC.";
    launch_pending_ = false;
    ReportFatalError();
  }
}

void WtsSessionProcessDelegate::Core::ReportFatalError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  channel_.reset();
  pipe_.Close();

  WorkerProcessLauncher* event_handler = event_handler_;
  event_handler_ = NULL;
  event_handler->OnFatalError();
}

void WtsSessionProcessDelegate::Core::ReportProcessLaunched(
    base::win::ScopedHandle worker_process) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!worker_process_.IsValid());

  worker_process_ = worker_process.Pass();

  // Report a handle that can be used to wait for the worker process completion,
  // query information about the process and duplicate handles.
  DWORD desired_access =
      SYNCHRONIZE | PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION;
  HANDLE temp_handle;
  if (!DuplicateHandle(GetCurrentProcess(),
                       worker_process_.Get(),
                       GetCurrentProcess(),
                       &temp_handle,
                       desired_access,
                       FALSE,
                       0)) {
    PLOG(ERROR) << "Failed to duplicate a handle";
    ReportFatalError();
    return;
  }
  ScopedHandle limited_handle(temp_handle);

  event_handler_->OnProcessLaunched(limited_handle.Pass());
}

WtsSessionProcessDelegate::WtsSessionProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<CommandLine> target_command,
    bool launch_elevated,
    const std::string& channel_security) {
  core_ = new Core(io_task_runner,
                   target_command.Pass(),
                   launch_elevated,
                   channel_security);
}

WtsSessionProcessDelegate::~WtsSessionProcessDelegate() {
  core_->Stop();
}

bool WtsSessionProcessDelegate::Initialize(uint32 session_id) {
  return core_->Initialize(session_id);
}

void WtsSessionProcessDelegate::LaunchProcess(
    WorkerProcessLauncher* event_handler) {
  core_->LaunchProcess(event_handler);
}

void WtsSessionProcessDelegate::Send(IPC::Message* message) {
  core_->Send(message);
}

void WtsSessionProcessDelegate::CloseChannel() {
  core_->CloseChannel();
}

void WtsSessionProcessDelegate::KillProcess() {
  core_->KillProcess();
}

}  // namespace remoting
