// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/win/wts_session_process_delegate.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/worker_process_launcher.h"
#include "remoting/host/win/wts_console_monitor.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::win::ScopedHandle;

// Name of the default session desktop.
const char kDefaultDesktopName[] = "winsta0\\default";

namespace remoting {

// A private class actually implementing the functionality provided by
// |WtsSessionProcessDelegate|. This class is ref-counted and implements
// asynchronous fire-and-forget shutdown.
class WtsSessionProcessDelegate::Core
    : public base::RefCountedThreadSafe<WtsSessionProcessDelegate::Core>,
      public base::MessagePumpForIO::IOHandler,
      public WorkerProcessLauncher::Delegate {
 public:
  // The caller must ensure that |delegate| remains valid at least until
  // Stop() method has been called.
  Core(scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
       scoped_ptr<CommandLine> target_command,
       bool launch_elevated,
       const std::string& channel_security);

  // base::MessagePumpForIO::IOHandler implementation.
  virtual void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                             DWORD bytes_transferred,
                             DWORD error) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // WorkerProcessLauncher::Delegate implementation.
  virtual DWORD GetProcessId() const OVERRIDE;
  virtual bool IsPermanentError(int failure_count) const OVERRIDE;
  virtual void KillProcess(DWORD exit_code) OVERRIDE;
  virtual bool LaunchProcess(
      IPC::Listener* delegate,
      base::win::ScopedHandle* process_exit_event_out) OVERRIDE;

  // Initializes the object returning true on success.
  bool Initialize(uint32 session_id);

  // Stops the object asynchronously.
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // Drains the completion port queue to make sure that all job object
  // notifications have been received.
  void DrainJobNotifications();

  // Notified that the completion port queue has been drained.
  void DrainJobNotificationsCompleted();

  // Creates and initializes the job object that will sandbox the launched child
  // processes.
  void InitializeJob();

  // Notified that the job object initialization is complete.
  void InitializeJobCompleted(scoped_ptr<base::win::ScopedHandle> job);

  // Called to process incoming job object notifications.
  void OnJobNotification(DWORD message, DWORD pid);

  // The task runner all public methods of this class should be called on.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Command line of the launched process.
  scoped_ptr<CommandLine> target_command_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  scoped_ptr<IPC::ChannelProxy> channel_;

  // Security descriptor (as SDDL) to be applied to |channel_|.
  std::string channel_security_;

  // Pointer to GetNamedPipeClientProcessId() API if it is available.
  typedef BOOL (WINAPI * GetNamedPipeClientProcessIdFn)(HANDLE, DWORD*);
  GetNamedPipeClientProcessIdFn get_named_pipe_client_pid_;

  // The job object used to control the lifetime of child processes.
  base::win::ScopedHandle job_;

  // True if the worker process should be launched elevated.
  bool launch_elevated_;

  // The named pipe used as the transport by |channel_|.
  base::win::ScopedHandle pipe_;

  // A handle that becomes signalled once all processes associated with the job
  // have been terminated.
  base::win::ScopedHandle process_exit_event_;

  // The token to be used to launch a process in a different session.
  base::win::ScopedHandle session_token_;

  // True if Stop() has been called.
  bool stopping_;

  // The handle of the worker process, if launched.
  base::win::ScopedHandle worker_process_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

WtsSessionProcessDelegate::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<CommandLine> target_command,
    bool launch_elevated,
    const std::string& channel_security)
    : main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner),
      target_command_(target_command.Pass()),
      channel_security_(channel_security),
      get_named_pipe_client_pid_(NULL),
      launch_elevated_(launch_elevated),
      stopping_(false) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void WtsSessionProcessDelegate::Core::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // |bytes_transferred| is used in job object notifications to supply
  // the message ID; |context| carries process ID.
  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Core::OnJobNotification, this, bytes_transferred,
      reinterpret_cast<DWORD>(context)));
}

bool WtsSessionProcessDelegate::Core::Send(IPC::Message* message) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (channel_.get()) {
    return channel_->Send(message);
  } else {
    delete message;
    return false;
  }
}

DWORD WtsSessionProcessDelegate::Core::GetProcessId() const {
  DWORD pid = 0;
  if (launch_elevated_ && pipe_.IsValid() &&
      get_named_pipe_client_pid_(pipe_, &pid)) {
    return pid;
  }

  if (worker_process_.IsValid())
    return ::GetProcessId(worker_process_);

  return 0;
}

bool WtsSessionProcessDelegate::Core::IsPermanentError(
    int failure_count) const {
  // Get exit code of the worker process if it is available.
  DWORD exit_code = CONTROL_C_EXIT;
  if (worker_process_.IsValid()) {
    if (!::GetExitCodeProcess(worker_process_, &exit_code)) {
      LOG_GETLASTERROR(INFO)
          << "Failed to query the exit code of the worker process";
      exit_code = CONTROL_C_EXIT;
    }
  }

  // Stop trying to restart the worker process if it exited due to
  // misconfiguration.
  return (kMinPermanentErrorExitCode <= exit_code &&
      exit_code <= kMaxPermanentErrorExitCode);
}

void WtsSessionProcessDelegate::Core::KillProcess(DWORD exit_code) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  channel_.reset();
  pipe_.Close();

  if (launch_elevated_) {
    if (job_.IsValid()) {
      TerminateJobObject(job_, exit_code);
    }
  } else {
    if (worker_process_.IsValid()) {
      TerminateProcess(worker_process_, exit_code);
    }
  }
}

bool WtsSessionProcessDelegate::Core::LaunchProcess(
    IPC::Listener* delegate,
    ScopedHandle* process_exit_event_out) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  CommandLine command_line(target_command_->argv());
  if (launch_elevated_) {
    // The job object is not ready. Retry starting the host process later.
    if (!job_.IsValid()) {
      return false;
    }

    // Construct the helper binary name.
    base::FilePath helper_binary;
    if (!GetInstalledBinaryPath(kHostBinaryName, &helper_binary))
      return false;

    // Create the command line passing the name of the IPC channel to use and
    // copying known switches from the caller's command line.
    command_line.SetProgram(helper_binary);
    command_line.AppendSwitchPath(kElevateSwitchName,
                                  target_command_->GetProgram());

    CHECK(ResetEvent(process_exit_event_));
  }

  // Create the server end of the IPC channel.
  std::string channel_name = IPC::Channel::GenerateUniqueRandomChannelID();
  ScopedHandle pipe;
  if (!CreateIpcChannel(channel_name, channel_security_, &pipe))
    return false;

  // Wrap the pipe into an IPC channel.
  scoped_ptr<IPC::ChannelProxy> channel(new IPC::ChannelProxy(
      IPC::ChannelHandle(pipe),
      IPC::Channel::MODE_SERVER,
      delegate,
      io_task_runner_));

  // Pass the name of the IPC channel to use.
  command_line.AppendSwitchNative(kDaemonPipeSwitchName,
                                  UTF8ToWide(channel_name));

  // Try to launch the process.
  ScopedHandle worker_process;
  ScopedHandle worker_thread;
  if (!LaunchProcessWithToken(command_line.GetProgram(),
                              command_line.GetCommandLineString(),
                              session_token_,
                              NULL,
                              NULL,
                              false,
                              CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB,
                              UTF8ToUTF16(kDefaultDesktopName).c_str(),
                              &worker_process,
                              &worker_thread)) {
    return false;
  }

  HANDLE local_process_exit_event;
  if (launch_elevated_) {
    if (!AssignProcessToJobObject(job_, worker_process)) {
      LOG_GETLASTERROR(ERROR)
          << "Failed to assign the worker to the job object";
      TerminateProcess(worker_process, CONTROL_C_EXIT);
      return false;
    }

    local_process_exit_event = process_exit_event_;
  } else {
    worker_process_ = worker_process.Pass();
    local_process_exit_event = worker_process_;
  }

  if (!ResumeThread(worker_thread)) {
    LOG_GETLASTERROR(ERROR) << "Failed to resume the worker thread";
    KillProcess(CONTROL_C_EXIT);
    return false;
  }

  // Return a handle that the caller can wait on to get notified when
  // the process terminates.
  ScopedHandle process_exit_event;
  if (!DuplicateHandle(GetCurrentProcess(),
                       local_process_exit_event,
                       GetCurrentProcess(),
                       process_exit_event.Receive(),
                       SYNCHRONIZE,
                       FALSE,
                       0)) {
    LOG_GETLASTERROR(ERROR) << "Failed to duplicate a handle";
    KillProcess(CONTROL_C_EXIT);
    return false;
  }

  channel_ = channel.Pass();
  pipe_ = pipe.Pass();
  *process_exit_event_out = process_exit_event.Pass();
  return true;
}

bool WtsSessionProcessDelegate::Core::Initialize(uint32 session_id) {
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

    process_exit_event_.Set(CreateEvent(NULL, TRUE, FALSE, NULL));
    if (!process_exit_event_.IsValid()) {
      LOG(ERROR) << "Failed to create a nameless event";
      return false;
    }

    // To receive job object notifications the job object is registered with
    // the completion port represented by |io_task_runner|. The registration has
    // to be done on the I/O thread because
    // MessageLoopForIO::RegisterJobObject() can only be called via
    // MessageLoopForIO::current().
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&Core::InitializeJob, this));
  }

  // Create a session token for the launched process.
  return CreateSessionToken(session_id, &session_token_);
}

void WtsSessionProcessDelegate::Core::Stop() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (!stopping_) {
    stopping_ = true;

    // Drain the completion queue to make sure all job object notifications have
    // been received.
    DrainJobNotificationsCompleted();
  }
}

WtsSessionProcessDelegate::Core::~Core() {
}

void WtsSessionProcessDelegate::Core::DrainJobNotifications() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // DrainJobNotifications() is posted after the job object is destroyed, so
  // by this time all notifications from the job object have been processed
  // already. Let the main thread know that the queue has been drained.
  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Core::DrainJobNotificationsCompleted, this));
}

void WtsSessionProcessDelegate::Core::DrainJobNotificationsCompleted() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  if (job_.IsValid()) {
    job_.Close();

    // Drain the completion queue to make sure all job object notification have
    // been received.
    io_task_runner_->PostTask(FROM_HERE, base::Bind(
        &Core::DrainJobNotifications, this));
  }
}

void WtsSessionProcessDelegate::Core::InitializeJob() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

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

  // Register to receive job notifications via the I/O thread's completion port.
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
  main_task_runner_->PostTask(FROM_HERE, base::Bind(
      &Core::InitializeJobCompleted, this, base::Passed(&job_wrapper)));
}

void WtsSessionProcessDelegate::Core::InitializeJobCompleted(
    scoped_ptr<ScopedHandle> job) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!job_.IsValid());

  job_ = job->Pass();
}

void WtsSessionProcessDelegate::Core::OnJobNotification(DWORD message,
                                                        DWORD pid) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

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

WtsSessionProcessDelegate::WtsSessionProcessDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<CommandLine> target_command,
    uint32 session_id,
    bool launch_elevated,
    const std::string& channel_security) {
  core_ = new Core(main_task_runner, io_task_runner, target_command.Pass(),
                   launch_elevated, channel_security);
  if (!core_->Initialize(session_id)) {
    core_->Stop();
    core_ = NULL;
  }
}

WtsSessionProcessDelegate::~WtsSessionProcessDelegate() {
  if (core_) {
    core_->Stop();
    core_ = NULL;
  }
}

bool WtsSessionProcessDelegate::Send(IPC::Message* message) {
  return core_->Send(message);
}

DWORD WtsSessionProcessDelegate::GetProcessId() const {
  if (!core_)
    return 0;

  return core_->GetProcessId();
}

bool WtsSessionProcessDelegate::IsPermanentError(int failure_count) const {
  if (!core_)
    return false;

  return core_->IsPermanentError(failure_count);
}

void WtsSessionProcessDelegate::KillProcess(DWORD exit_code) {
  if (core_)
    core_->KillProcess(exit_code);
}

bool WtsSessionProcessDelegate::LaunchProcess(
    IPC::Listener* delegate,
    base::win::ScopedHandle* process_exit_event_out) {
  if (!core_)
    return false;

  return core_->LaunchProcess(delegate, process_exit_event_out);
}

} // namespace remoting
