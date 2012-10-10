// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/worker_process_launcher.h"

#include <sddl.h>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_message.h"
#include "net/base/backoff_entry.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::win::ScopedHandle;
using base::TimeDelta;

// Match the pipe name prefix used by Chrome IPC channels so that the client
// could use Chrome IPC APIs instead of connecting manually.
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome.";

// The minimum and maximum delays between attempts to inject host process into
// a session.
const int kMaxLaunchDelaySeconds = 60;
const int kMinLaunchDelaySeconds = 1;

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  1000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  60000,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};


namespace remoting {

// Launches a worker process that is controlled via an IPC channel. All
// interaction with the spawned process is through the IPC::Listener and Send()
// method. In case of error the channel is closed and the worker process is
// terminated.
class WorkerProcessLauncher::Core
    : public base::RefCountedThreadSafe<WorkerProcessLauncher::Core>,
      public base::win::ObjectWatcher::Delegate,
      public IPC::Listener {
 public:
  // Creates the launcher that will use |launcher_delegate| to manage the worker
  // process and |worker_delegate| to handle IPCs. The caller must ensure that
  // |worker_delegate| remains valid until Stoppable::Stop() method has been
  // called.
  //
  // The caller should call all the methods on this class on
  // the |caller_task_runner| thread. Methods of both delegate interfaces are
  // called on the |caller_task_runner| thread as well. |io_task_runner| is used
  // to perform background IPC I/O.
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
       scoped_ptr<WorkerProcessLauncher::Delegate> launcher_delegate,
       WorkerProcessIpcDelegate* worker_delegate,
       const std::string& pipe_security_descriptor);

  // Launches the worker process.
  void Start();

  // Stops the worker process asynchronously. The caller can drop the reference
  // to |this| as soon as Stop() returns.
  void Stop();

  // Sends an IPC message to the worker process. The message will be silently
  // dropped if Send() is called before Start() or after stutdown has been
  // initiated.
  void Send(IPC::Message* message);

  // base::win::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // Creates the server end of the Chromoting IPC channel.
  bool CreatePipeForIpcChannel(const std::string& channel_name,
                               const std::string& pipe_security_descriptor,
                               base::win::ScopedHandle* pipe_out);

  // Generates random channel ID.
  std::string GenerateRandomChannelId();

  // Attempts to launch the worker process. Schedules next launch attempt if
  // creation of the process fails.
  void LaunchWorker();

  // Records a successfull launch attempt.
  void RecordSuccessfullLaunch();

  // Stops the worker process asynchronously and schedules next launch attempt
  // unless Stop() has been called already.
  void StopWorker();

  // All public methods are called on the |caller_task_runner| thread.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // The task runner is used perform background IPC I/O.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Implements specifics of launching a worker process.
  scoped_ptr<WorkerProcessLauncher::Delegate> launcher_delegate_;

  // Handles IPC messages sent by the worker process.
  WorkerProcessIpcDelegate* worker_delegate_;

  // The IPC channel connecting to the launched process.
  scoped_ptr<IPC::ChannelProxy> ipc_channel_;

  // The timer used to delay termination of the process in the case of an IPC
  // error.
  scoped_ptr<base::OneShotTimer<Core> > ipc_error_timer_;

  // Launch backoff state.
  net::BackoffEntry launch_backoff_;

  // Timer used to delay recording a successfull launch.
  scoped_ptr<base::OneShotTimer<Core> > launch_success_timer_;

  // Timer used to schedule the next attempt to launch the process.
  scoped_ptr<base::OneShotTimer<Core> > launch_timer_;

  // Used to determine when the launched process terminates.
  base::win::ObjectWatcher process_watcher_;

  // A waiting handle that becomes signalled once the launched process has
  // been terminated.
  base::win::ScopedHandle process_exit_event_;

  // The server end of the pipe.
  base::win::ScopedHandle pipe_;

  // The security descriptor (as SDDL) of the server end of the pipe.
  std::string pipe_security_descriptor_;

  // Self reference to keep the object alive while the worker process is being
  // terminated.
  scoped_refptr<Core> self_;

  // True when Stop() has been called.
  bool stopping_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

WorkerProcessLauncher::Delegate::~Delegate() {
}

WorkerProcessLauncher::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<WorkerProcessLauncher::Delegate> launcher_delegate,
    WorkerProcessIpcDelegate* worker_delegate,
    const std::string& pipe_security_descriptor)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      launcher_delegate_(launcher_delegate.Pass()),
      worker_delegate_(worker_delegate),
      launch_backoff_(&kDefaultBackoffPolicy),
      pipe_security_descriptor_(pipe_security_descriptor),
      stopping_(false) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // base::OneShotTimer must be destroyed on the same thread it was created on.
  ipc_error_timer_.reset(new base::OneShotTimer<Core>());
  launch_success_timer_.reset(new base::OneShotTimer<Core>());
  launch_timer_.reset(new base::OneShotTimer<Core>());
}

void WorkerProcessLauncher::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!stopping_);

  LaunchWorker();
}

void WorkerProcessLauncher::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!stopping_) {
    stopping_ = true;
    worker_delegate_ = NULL;
    StopWorker();
  }
}

void WorkerProcessLauncher::Core::Send(IPC::Message* message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (ipc_channel_.get()) {
    ipc_channel_->Send(message);
  } else {
    delete message;
  }
}

void WorkerProcessLauncher::Core::OnObjectSignaled(HANDLE object) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  StopWorker();
}

bool WorkerProcessLauncher::Core::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() != NULL);

  return worker_delegate_->OnMessageReceived(message);
}

void WorkerProcessLauncher::Core::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() != NULL);

  // |peer_pid| is send by the client and cannot be trusted.
  // GetNamedPipeClientProcessId() is not available on XP. The pipe's security
  // descriptor is the only protection we currently have against malicious
  // clients.
  //
  // If we'd like to be able to launch low-privileged workers and let them
  // connect back, the pipe handle should be passed to the worker instead of
  // the pipe name.
  worker_delegate_->OnChannelConnected();
}

void WorkerProcessLauncher::Core::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() != NULL);

  // Schedule a delayed termination of the worker process. Usually, the pipe is
  // disconnected when the worker process is about to exit. Waiting a little bit
  // here allows the worker to exit completely and so, notify
  // |process_watcher_|. As the result KillProcess() will not be called and
  // the original exit code reported by the worker process will be retrieved.
  ipc_error_timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(5),
                          this, &Core::StopWorker);
}

WorkerProcessLauncher::Core::~Core() {
  DCHECK(stopping_);
}

// Creates the server end of the Chromoting IPC channel.
bool WorkerProcessLauncher::Core::CreatePipeForIpcChannel(
    const std::string& channel_name,
    const std::string& pipe_security_descriptor,
    ScopedHandle* pipe_out) {
  // Create security descriptor for the channel.
  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = FALSE;

  ULONG security_descriptor_length = 0;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptor(
          UTF8ToUTF16(pipe_security_descriptor).c_str(),
          SDDL_REVISION_1,
          reinterpret_cast<PSECURITY_DESCRIPTOR*>(
              &security_attributes.lpSecurityDescriptor),
          &security_descriptor_length)) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create a security descriptor for the Chromoting IPC channel";
    return false;
  }

  // Convert the channel name to the pipe name.
  std::string pipe_name(kChromePipeNamePrefix);
  pipe_name.append(channel_name);

  // Create the server end of the pipe. This code should match the code in
  // IPC::Channel with exception of passing a non-default security descriptor.
  base::win::ScopedHandle pipe;
  pipe.Set(CreateNamedPipe(
      UTF8ToUTF16(pipe_name).c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
      1,
      IPC::Channel::kReadBufferSize,
      IPC::Channel::kReadBufferSize,
      5000,
      &security_attributes));
  if (!pipe.IsValid()) {
    LOG_GETLASTERROR(ERROR) <<
        "Failed to create the server end of the Chromoting IPC channel";
    LocalFree(security_attributes.lpSecurityDescriptor);
    return false;
  }

  LocalFree(security_attributes.lpSecurityDescriptor);

  *pipe_out = pipe.Pass();
  return true;
}

// N.B. Copied from src/content/common/child_process_host_impl.cc
std::string WorkerProcessLauncher::Core::GenerateRandomChannelId() {
  return base::StringPrintf("%d.%p.%d",
                            base::GetCurrentProcId(), this,
                            base::RandInt(0, std::numeric_limits<int>::max()));
}

void WorkerProcessLauncher::Core::LaunchWorker() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(ipc_channel_.get() == NULL);
  DCHECK(!launch_success_timer_->IsRunning());
  DCHECK(!launch_timer_->IsRunning());
  DCHECK(!pipe_.IsValid());
  DCHECK(!process_exit_event_.IsValid());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  std::string channel_name = GenerateRandomChannelId();
  if (CreatePipeForIpcChannel(channel_name, pipe_security_descriptor_,
                              &pipe_)) {
    // Wrap the pipe into an IPC channel.
    ipc_channel_.reset(new IPC::ChannelProxy(
        IPC::ChannelHandle(pipe_),
        IPC::Channel::MODE_SERVER,
        this,
        io_task_runner_));

    // Launch the process and attach an object watcher to the returned process
    // handle so that we get notified if the process terminates.
    if (launcher_delegate_->LaunchProcess(channel_name, &process_exit_event_)) {
      if (process_watcher_.StartWatching(process_exit_event_, this)) {
        // Record a successful launch once the process has been running for at
        // least two seconds.
        launch_success_timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(2),
                                     this, &Core::RecordSuccessfullLaunch);
        return;
      }

      launcher_delegate_->KillProcess(CONTROL_C_EXIT);
    }
  }

  launch_backoff_.InformOfRequest(false);
  StopWorker();
}

void WorkerProcessLauncher::Core::RecordSuccessfullLaunch() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launch_backoff_.InformOfRequest(true);
}

void WorkerProcessLauncher::Core::StopWorker() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Record a launch failure if the process exited too soon.
  if (launch_success_timer_->IsRunning()) {
    launch_success_timer_->Stop();
    launch_backoff_.InformOfRequest(false);
  }

  // Keep |this| alive until the worker process is terminated.
  self_ = this;

  ipc_channel_.reset();
  pipe_.Close();

  // Kill the process if it has been started already.
  if (process_watcher_.GetWatchedObject() != NULL) {
    launcher_delegate_->KillProcess(CONTROL_C_EXIT);
    return;
  }

  ipc_error_timer_->Stop();

  DCHECK(ipc_channel_.get() == NULL);
  DCHECK(!pipe_.IsValid());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  process_exit_event_.Close();

  // Do not relaunch the worker process if the caller has asked us to stop.
  if (stopping_) {
    ipc_error_timer_.reset();
    launch_timer_.reset();
    self_ = NULL;
    return;
  }

  self_ = NULL;

  // Stop trying to restart the worker process if it exited due to
  // misconfiguration.
  DWORD exit_code = launcher_delegate_->GetExitCode();
  if (kMinPermanentErrorExitCode <= exit_code &&
      exit_code <= kMaxPermanentErrorExitCode) {
    // |delegate_| must be valid because Stop() hasn't been called yet and
    // |running_| is true. |worker_delegate_| is valid here because Stop()
    // hasn't been called yet (|stopping_| is false).
    worker_delegate_->OnPermanentError();
    return;
  }

  // Schedule the next attempt to launch the worker process.
  launch_timer_->Start(FROM_HERE, launch_backoff_.GetTimeUntilRelease(),
                       this, &Core::LaunchWorker);
}

WorkerProcessLauncher::WorkerProcessLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_ptr<Delegate> launcher_delegate,
    WorkerProcessIpcDelegate* worker_delegate,
    const std::string& pipe_security_descriptor) {
  core_ = new Core(caller_task_runner, io_task_runner, launcher_delegate.Pass(),
                   worker_delegate, pipe_security_descriptor);
  core_->Start();
}

WorkerProcessLauncher::~WorkerProcessLauncher() {
  core_->Stop();
  core_ = NULL;
}

void WorkerProcessLauncher::Send(IPC::Message* message) {
  core_->Send(message);
}

} // namespace remoting
