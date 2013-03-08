// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/worker_process_launcher.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/win/object_watcher.h"
#include "base/win/windows_version.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "net/base/backoff_entry.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/worker_process_ipc_delegate.h"

using base::TimeDelta;
using base::win::ScopedHandle;

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

const int kKillProcessTimeoutSeconds = 5;
const int kLaunchSuccessTimeoutSeconds = 2;

namespace remoting {

// Launches a worker process that is controlled via an IPC channel. All
// interaction with the spawned process is through the IPC::Listener and Send()
// method. In case of error the channel is closed and the worker process is
// terminated.
class WorkerProcessLauncher::Core
    : public base::RefCountedThreadSafe<Core, Core>,
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
  // called on the |caller_task_runner| thread as well.
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_ptr<WorkerProcessLauncher::Delegate> launcher_delegate,
       WorkerProcessIpcDelegate* worker_delegate);

  // Launches the worker process.
  void Start();

  // Stops the worker process asynchronously. The caller can drop the reference
  // to |this| as soon as Stop() returns.
  void Stop();

  // See WorkerProcessLauncher::Crash().
  void Crash(const tracked_objects::Location& location);

  // See WorkerProcessLauncher::Send().
  void Send(IPC::Message* message);

  // Used to emulate |launch_success_timer_| expiration.
  void ResetLaunchSuccessTimeoutForTest();

  // Set the desired timeout for |kill_process_timer_|.
  void SetKillProcessTimeoutForTest(const base::TimeDelta& timeout);

  // base::win::ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Destroys |Core| instances on the |caller_task_runner_| thread. This is
  // needed because base::OneShotTimer instances must be destroyed on the same
  // thread they were  created on.
  static void Destruct(const Core* core);

 private:
  friend class base::DeleteHelper<Core>;
  virtual ~Core();

  // Attempts to launch the worker process. Schedules next launch attempt if
  // creation of the process fails.
  void LaunchWorker();

  // Records a successful launch attempt.
  void RecordSuccessfulLaunch();

  // Stops the worker process asynchronously and schedules next launch attempt
  // unless Stop() has been called already.
  void StopWorker();

  // All public methods are called on the |caller_task_runner| thread.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Implements specifics of launching a worker process.
  scoped_ptr<WorkerProcessLauncher::Delegate> launcher_delegate_;

  // Handles IPC messages sent by the worker process.
  WorkerProcessIpcDelegate* worker_delegate_;

  // Pointer to GetNamedPipeClientProcessId() API if it is available.
  typedef BOOL (WINAPI * GetNamedPipeClientProcessIdFn)(HANDLE, DWORD*);
  GetNamedPipeClientProcessIdFn get_named_pipe_client_pid_;

  // True if IPC messages should be passed to |worker_delegate_|.
  bool ipc_enabled_;

  // The timer used to delay termination of the worker process when an IPC error
  // occured or when Crash() request is pending
  base::OneShotTimer<Core> kill_process_timer_;

  // The default timeout for |kill_process_timer_|.
  base::TimeDelta kill_process_timeout_;

  // Launch backoff state.
  net::BackoffEntry launch_backoff_;

  // Timer used to delay recording a successfull launch.
  base::OneShotTimer<Core> launch_success_timer_;

  // Timer used to schedule the next attempt to launch the process.
  base::OneShotTimer<Core> launch_timer_;

  // Used to determine when the launched process terminates.
  base::win::ObjectWatcher process_watcher_;

  // A waiting handle that becomes signalled once the launched process has
  // been terminated.
  ScopedHandle process_exit_event_;

  // True when Stop() has been called.
  bool stopping_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

WorkerProcessLauncher::Delegate::~Delegate() {
}

WorkerProcessLauncher::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_ptr<WorkerProcessLauncher::Delegate> launcher_delegate,
    WorkerProcessIpcDelegate* worker_delegate)
    : caller_task_runner_(caller_task_runner),
      launcher_delegate_(launcher_delegate.Pass()),
      worker_delegate_(worker_delegate),
      get_named_pipe_client_pid_(NULL),
      ipc_enabled_(false),
      kill_process_timeout_(
          base::TimeDelta::FromSeconds(kKillProcessTimeoutSeconds)),
      launch_backoff_(&kDefaultBackoffPolicy),
      stopping_(false) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
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

void WorkerProcessLauncher::Core::Crash(
    const tracked_objects::Location& location) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Ask the worker process to crash voluntarily if it is still connected.
  if (ipc_enabled_) {
    Send(new ChromotingDaemonMsg_Crash(location.function_name(),
                                       location.file_name(),
                                       location.line_number()));
  }

  // Close the channel and ignore any not yet processed messages.
  launcher_delegate_->CloseChannel();
  ipc_enabled_ = false;

  // Give the worker process some time to crash.
  if (!kill_process_timer_.IsRunning()) {
    kill_process_timer_.Start(FROM_HERE, kill_process_timeout_, this,
                              &Core::StopWorker);
  }
}

void WorkerProcessLauncher::Core::Send(IPC::Message* message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (ipc_enabled_) {
    launcher_delegate_->Send(message);
  } else {
    delete message;
  }
}

void WorkerProcessLauncher::Core::ResetLaunchSuccessTimeoutForTest() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (launch_success_timer_.IsRunning()) {
    launch_success_timer_.Stop();
    RecordSuccessfulLaunch();
  }
}

void WorkerProcessLauncher::Core::SetKillProcessTimeoutForTest(
    const base::TimeDelta& timeout) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  kill_process_timeout_ = timeout;
}

void WorkerProcessLauncher::Core::OnObjectSignaled(HANDLE object) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  StopWorker();
}

bool WorkerProcessLauncher::Core::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!ipc_enabled_)
    return false;

  return worker_delegate_->OnMessageReceived(message);
}

void WorkerProcessLauncher::Core::OnChannelConnected(int32 peer_pid) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!ipc_enabled_)
    return;

  // Verify |peer_pid| because it is controlled by the client and cannot be
  // trusted.
  DWORD actual_pid = launcher_delegate_->GetProcessId();
  if (peer_pid != static_cast<int32>(actual_pid)) {
    LOG(ERROR) << "The actual client PID " << actual_pid
               << " does not match the one reported by the client: "
               << peer_pid;
    StopWorker();
    return;
  }

  // This can result in |this| being deleted, so this call must be the last in
  // this method.
  worker_delegate_->OnChannelConnected(peer_pid);
}

void WorkerProcessLauncher::Core::OnChannelError() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Schedule a delayed termination of the worker process. Usually, the pipe is
  // disconnected when the worker process is about to exit. Waiting a little bit
  // here allows the worker to exit completely and so, notify
  // |process_watcher_|. As the result KillProcess() will not be called and
  // the original exit code reported by the worker process will be retrieved.
  if (!kill_process_timer_.IsRunning()) {
    kill_process_timer_.Start(FROM_HERE, kill_process_timeout_, this,
                              &Core::StopWorker);
  }
}

// static
void WorkerProcessLauncher::Core::Destruct(const Core* core) {
  core->caller_task_runner_->DeleteSoon(FROM_HERE, core);
}

WorkerProcessLauncher::Core::~Core() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(stopping_);
}

void WorkerProcessLauncher::Core::LaunchWorker() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!ipc_enabled_);
  DCHECK(!launch_success_timer_.IsRunning());
  DCHECK(!launch_timer_.IsRunning());
  DCHECK(!process_exit_event_.IsValid());
  DCHECK(process_watcher_.GetWatchedObject() == NULL);

  // Launch the process and attach an object watcher to the returned process
  // handle so that we get notified if the process terminates.
  if (launcher_delegate_->LaunchProcess(this, &process_exit_event_)) {
    if (process_watcher_.StartWatching(process_exit_event_, this)) {
      ipc_enabled_ = true;
      // Record a successful launch once the process has been running for at
      // least two seconds.
      launch_success_timer_.Start(
          FROM_HERE, base::TimeDelta::FromSeconds(kLaunchSuccessTimeoutSeconds),
          this, &Core::RecordSuccessfulLaunch);
      return;
    }

    launcher_delegate_->KillProcess(CONTROL_C_EXIT);
  }

  launch_backoff_.InformOfRequest(false);
  StopWorker();
}

void WorkerProcessLauncher::Core::RecordSuccessfulLaunch() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  launch_backoff_.InformOfRequest(true);
}

void WorkerProcessLauncher::Core::StopWorker() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  // Keep the object alive in case one of delegates decides to delete |this|.
  scoped_refptr<Core> self = this;

  // Record a launch failure if the process exited too soon.
  if (launch_success_timer_.IsRunning()) {
    launch_success_timer_.Stop();
    launch_backoff_.InformOfRequest(false);
  }

  // Ignore any remaining IPC messages.
  ipc_enabled_ = false;

  // Kill the process if it has been started already.
  if (process_watcher_.GetWatchedObject() != NULL) {
    launcher_delegate_->KillProcess(CONTROL_C_EXIT);

    // Wait until the process is actually stopped if the caller keeps
    // a reference to |this|. Otherwise terminate everything right now - there
    // won't be a second chance.
    if (!stopping_)
      return;

    process_watcher_.StopWatching();
  }

  kill_process_timer_.Stop();
  process_exit_event_.Close();

  // Do not relaunch the worker process if the caller has asked us to stop.
  if (stopping_)
    return;

  if (launcher_delegate_->IsPermanentError(launch_backoff_.failure_count())) {
    if (!stopping_)
      worker_delegate_->OnPermanentError();
  } else {
    // Schedule the next attempt to launch the worker process.
    launch_timer_.Start(FROM_HERE, launch_backoff_.GetTimeUntilRelease(), this,
                        &Core::LaunchWorker);
  }
}

WorkerProcessLauncher::WorkerProcessLauncher(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_ptr<Delegate> launcher_delegate,
    WorkerProcessIpcDelegate* worker_delegate) {
  core_ = new Core(caller_task_runner, launcher_delegate.Pass(),
                   worker_delegate);
  core_->Start();
}

WorkerProcessLauncher::~WorkerProcessLauncher() {
  core_->Stop();
  core_ = NULL;
}

void WorkerProcessLauncher::Crash(const tracked_objects::Location& location) {
  core_->Crash(location);
}

void WorkerProcessLauncher::Send(IPC::Message* message) {
  core_->Send(message);
}

void WorkerProcessLauncher::ResetLaunchSuccessTimeoutForTest() {
  core_->ResetLaunchSuccessTimeoutForTest();
}

void WorkerProcessLauncher::SetKillProcessTimeoutForTest(
    const base::TimeDelta& timeout) {
  core_->SetKillProcessTimeoutForTest(timeout);
}


} // namespace remoting
