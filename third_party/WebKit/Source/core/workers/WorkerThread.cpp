/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/workers/WorkerThread.h"

#include <limits.h>
#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/probe/CoreProbes.h"
#include "core/workers/GlobalScopeCreationParams.h"
#include "core/workers/InstalledScriptsManager.h"
#include "core/workers/ThreadedWorkletGlobalScope.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/bindings/Microtask.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "platform/scheduler/child/worker_global_scope_scheduler.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"

namespace blink {

using ExitCode = WorkerThread::ExitCode;

namespace {

// TODO(nhiroki): Adjust the delay based on UMA.
constexpr TimeDelta kForcibleTerminationDelay = TimeDelta::FromSeconds(2);

void ForwardInterfaceRequest(const std::string& name,
                             mojo::ScopedMessagePipeHandle handle) {
  Platform::Current()->GetInterfaceProvider()->GetInterface(name.c_str(),
                                                            std::move(handle));
}

}  // namespace

static Mutex& ThreadSetMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

static int GetNextWorkerThreadId() {
  DCHECK(IsMainThread());
  static int next_worker_thread_id = 1;
  CHECK_LT(next_worker_thread_id, std::numeric_limits<int>::max());
  return next_worker_thread_id++;
}

WorkerThread::~WorkerThread() {
  DCHECK(IsMainThread());
  MutexLocker lock(ThreadSetMutex());
  DCHECK(WorkerThreads().Contains(this));
  WorkerThreads().erase(this);

  DCHECK_NE(ExitCode::kNotTerminated, exit_code_);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, exit_code_histogram,
      ("WorkerThread.ExitCode", static_cast<int>(ExitCode::kLastEnum)));
  exit_code_histogram.Count(static_cast<int>(exit_code_));
}

void WorkerThread::Start(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const WTF::Optional<WorkerBackingThreadStartupData>& thread_startup_data,
    ParentFrameTaskRunners* parent_frame_task_runners) {
  DCHECK(IsMainThread());
  DCHECK(!parent_frame_task_runners_);
  parent_frame_task_runners_ = parent_frame_task_runners;

  // Synchronously initialize the per-global-scope scheduler to prevent someone
  // from posting a task to the thread before the scheduler is ready.
  WaitableEvent waitable_event;
  GetWorkerBackingThread().BackingThread().PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThread::InitializeSchedulerOnWorkerThread,
                      CrossThreadUnretained(this),
                      CrossThreadUnretained(&waitable_event)));
  waitable_event.Wait();

  GetWorkerBackingThread().BackingThread().PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThread::InitializeOnWorkerThread,
                      CrossThreadUnretained(this),
                      WTF::Passed(std::move(global_scope_creation_params)),
                      thread_startup_data));
}

void WorkerThread::Terminate() {
  DCHECK(IsMainThread());

  {
    MutexLocker lock(thread_state_mutex_);
    if (requested_to_terminate_)
      return;
    requested_to_terminate_ = true;
  }

  // Schedule a task to forcibly terminate the script execution in case that the
  // shutdown sequence does not start on the worker thread in a certain time
  // period.
  ScheduleToTerminateScriptExecution();

  worker_thread_lifecycle_context_->NotifyContextDestroyed();
  inspector_task_runner_->Kill();

  GetWorkerBackingThread().BackingThread().PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThread::PrepareForShutdownOnWorkerThread,
                      CrossThreadUnretained(this)));
  GetWorkerBackingThread().BackingThread().PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerThread::PerformShutdownOnWorkerThread,
                      CrossThreadUnretained(this)));
}

void WorkerThread::TerminateAllWorkersForTesting() {
  DCHECK(IsMainThread());

  // Keep this lock to prevent WorkerThread instances from being destroyed.
  MutexLocker lock(ThreadSetMutex());
  HashSet<WorkerThread*> threads = WorkerThreads();

  for (WorkerThread* thread : threads) {
    // Schedule a regular async worker thread termination task, and forcibly
    // terminate the V8 script execution to ensure the task runs.
    thread->Terminate();
    thread->EnsureScriptExecutionTerminates(ExitCode::kSyncForciblyTerminated);
  }

  for (WorkerThread* thread : threads)
    thread->shutdown_event_->Wait();

  // Destruct base::Thread and join the underlying system threads.
  for (WorkerThread* thread : threads)
    thread->ClearWorkerBackingThread();
}

void WorkerThread::WillProcessTask() {
  DCHECK(IsCurrentThread());

  // No tasks should get executed after we have closed.
  DCHECK(!GlobalScope()->IsClosing());
}

void WorkerThread::DidProcessTask() {
  DCHECK(IsCurrentThread());
  Microtask::PerformCheckpoint(GetIsolate());
  GlobalScope()->ScriptController()->GetRejectedPromises()->ProcessQueue();
  if (GlobalScope()->IsClosing()) {
    // This WorkerThread will eventually be requested to terminate.
    GetWorkerReportingProxy().DidCloseWorkerGlobalScope();

    // Stop further worker tasks to run after this point.
    PrepareForShutdownOnWorkerThread();
  } else if (IsForciblyTerminated()) {
    // The script has been terminated forcibly, which means we need to
    // ask objects in the thread to stop working as soon as possible.
    PrepareForShutdownOnWorkerThread();
  }
}

v8::Isolate* WorkerThread::GetIsolate() {
  return GetWorkerBackingThread().GetIsolate();
}

bool WorkerThread::IsCurrentThread() {
  return GetWorkerBackingThread().BackingThread().IsCurrentThread();
}

ThreadableLoadingContext* WorkerThread::GetLoadingContext() {
  DCHECK(IsCurrentThread());
  // This should be never called after the termination sequence starts.
  DCHECK(loading_context_);
  return loading_context_;
}

void WorkerThread::AppendDebuggerTask(CrossThreadClosure task) {
  DCHECK(IsMainThread());
  if (requested_to_terminate_)
    return;
  inspector_task_runner_->AppendTask(CrossThreadBind(
      &WorkerThread::PerformDebuggerTaskOnWorkerThread,
      CrossThreadUnretained(this), WTF::Passed(std::move(task))));
  {
    MutexLocker lock(thread_state_mutex_);
    if (GetIsolate() && thread_state_ != ThreadState::kReadyToShutdown)
      inspector_task_runner_->InterruptAndRunAllTasksDontWait(GetIsolate());
  }
  TaskRunnerHelper::Get(TaskType::kUnthrottled, this)
      ->PostTask(BLINK_FROM_HERE,
                 CrossThreadBind(
                     &WorkerThread::PerformDebuggerTaskDontWaitOnWorkerThread,
                     CrossThreadUnretained(this)));
}

void WorkerThread::StartRunningDebuggerTasksOnPauseOnWorkerThread() {
  DCHECK(IsCurrentThread());
  if (worker_inspector_controller_)
    worker_inspector_controller_->FlushProtocolNotifications();
  paused_in_debugger_ = true;
  ThreadDebugger::IdleStarted(GetIsolate());
  CrossThreadClosure task;
  do {
    task =
        inspector_task_runner_->TakeNextTask(InspectorTaskRunner::kWaitForTask);
    if (task)
      task();
    // Keep waiting until execution is resumed.
  } while (task && paused_in_debugger_);
  ThreadDebugger::IdleFinished(GetIsolate());
}

void WorkerThread::StopRunningDebuggerTasksOnPauseOnWorkerThread() {
  DCHECK(IsCurrentThread());
  paused_in_debugger_ = false;
}

WorkerOrWorkletGlobalScope* WorkerThread::GlobalScope() {
  DCHECK(IsCurrentThread());
  return global_scope_.Get();
}

WorkerInspectorController* WorkerThread::GetWorkerInspectorController() {
  DCHECK(IsCurrentThread());
  return worker_inspector_controller_.Get();
}

unsigned WorkerThread::WorkerThreadCount() {
  MutexLocker lock(ThreadSetMutex());
  return WorkerThreads().size();
}

HashSet<WorkerThread*>& WorkerThread::WorkerThreads() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
  return threads;
}

PlatformThreadId WorkerThread::GetPlatformThreadId() {
  return GetWorkerBackingThread().BackingThread().PlatformThread().ThreadId();
}

bool WorkerThread::IsForciblyTerminated() {
  MutexLocker lock(thread_state_mutex_);
  switch (exit_code_) {
    case ExitCode::kNotTerminated:
    case ExitCode::kGracefullyTerminated:
      return false;
    case ExitCode::kSyncForciblyTerminated:
    case ExitCode::kAsyncForciblyTerminated:
      return true;
    case ExitCode::kLastEnum:
      NOTREACHED() << static_cast<int>(exit_code_);
      return false;
  }
  NOTREACHED() << static_cast<int>(exit_code_);
  return false;
}

ExitCode WorkerThread::GetExitCodeForTesting() {
  MutexLocker lock(thread_state_mutex_);
  return exit_code_;
}

service_manager::InterfaceProvider& WorkerThread::GetInterfaceProvider() {
  // TODO(https://crbug.com/734210): Instead of forwarding to the process-wide
  // interface provider a worker-specific interface provider pipe should be
  // passed in as part of the WorkerThreadStartupData.
  return interface_provider_;
}

WorkerThread::WorkerThread(ThreadableLoadingContext* loading_context,
                           WorkerReportingProxy& worker_reporting_proxy)
    : time_origin_(MonotonicallyIncreasingTime()),
      worker_thread_id_(GetNextWorkerThreadId()),
      forcible_termination_delay_(kForcibleTerminationDelay),
      inspector_task_runner_(WTF::MakeUnique<InspectorTaskRunner>()),
      loading_context_(loading_context),
      worker_reporting_proxy_(worker_reporting_proxy),
      shutdown_event_(WTF::WrapUnique(
          new WaitableEvent(WaitableEvent::ResetPolicy::kManual,
                            WaitableEvent::InitialState::kNonSignaled))),
      worker_thread_lifecycle_context_(new WorkerThreadLifecycleContext) {
  DCHECK(IsMainThread());
  MutexLocker lock(ThreadSetMutex());
  WorkerThreads().insert(this);
  interface_provider_.Forward(
      ConvertToBaseCallback(WTF::Bind(&ForwardInterfaceRequest)));
}

void WorkerThread::ScheduleToTerminateScriptExecution() {
  DCHECK(!forcible_termination_task_handle_.IsActive());
  forcible_termination_task_handle_ =
      parent_frame_task_runners_->Get(TaskType::kUnspecedTimer)
          ->PostDelayedCancellableTask(
              BLINK_FROM_HERE,
              WTF::Bind(&WorkerThread::EnsureScriptExecutionTerminates,
                        WTF::Unretained(this),
                        ExitCode::kAsyncForciblyTerminated),
              forcible_termination_delay_);
}

bool WorkerThread::ShouldTerminateScriptExecution(const MutexLocker& lock) {
  DCHECK(IsMainThread());
  DCHECK(IsThreadStateMutexLocked(lock));

  switch (thread_state_) {
    case ThreadState::kNotStarted:
      // Shutdown sequence will surely start during initialization sequence
      // on the worker thread. Don't have to schedule a termination task.
      return false;
    case ThreadState::kRunning:
      // Terminating during debugger task may lead to crash due to heavy use
      // of v8 api in debugger. Any debugger task is guaranteed to finish, so
      // we can wait for the completion.
      return !running_debugger_task_;
    case ThreadState::kReadyToShutdown:
      // Shutdown sequence will surely start soon. Don't have to schedule a
      // termination task.
      return false;
  }
  NOTREACHED();
  return false;
}

void WorkerThread::EnsureScriptExecutionTerminates(ExitCode exit_code) {
  DCHECK(IsMainThread());
  MutexLocker lock(thread_state_mutex_);
  if (!ShouldTerminateScriptExecution(lock))
    return;

  DCHECK(exit_code == ExitCode::kSyncForciblyTerminated ||
         exit_code == ExitCode::kAsyncForciblyTerminated);
  SetExitCode(lock, exit_code);

  GetIsolate()->TerminateExecution();
  forcible_termination_task_handle_.Cancel();
}

void WorkerThread::InitializeSchedulerOnWorkerThread(
    WaitableEvent* waitable_event) {
  DCHECK(IsCurrentThread());
  DCHECK(!global_scope_scheduler_);
  scheduler::WebThreadImplForWorkerScheduler& web_thread_for_worker =
      static_cast<scheduler::WebThreadImplForWorkerScheduler&>(
          GetWorkerBackingThread().BackingThread().PlatformThread());
  global_scope_scheduler_ =
      WTF::MakeUnique<scheduler::WorkerGlobalScopeScheduler>(
          web_thread_for_worker.GetWorkerScheduler());
  waitable_event->Signal();
}

void WorkerThread::InitializeOnWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const WTF::Optional<WorkerBackingThreadStartupData>& thread_startup_data) {
  DCHECK(IsCurrentThread());
  DCHECK_EQ(ThreadState::kNotStarted, thread_state_);

  KURL script_url = global_scope_creation_params->script_url;
  // TODO(nhiroki): Rename WorkerThreadStartMode to GlobalScopeStartMode.
  // (https://crbug.com/710364)
  WorkerThreadStartMode start_mode = global_scope_creation_params->start_mode;
  V8CacheOptions v8_cache_options =
      global_scope_creation_params->v8_cache_options;

  String source_code;
  std::unique_ptr<Vector<char>> cached_meta_data;
  bool should_terminate = false;
  if (RuntimeEnabledFeatures::ServiceWorkerScriptStreamingEnabled() &&
      GetInstalledScriptsManager() &&
      GetInstalledScriptsManager()->IsScriptInstalled(script_url)) {
    // GetScriptData blocks until the script is received from the browser.
    InstalledScriptsManager::ScriptData script_data;
    InstalledScriptsManager::ScriptStatus status =
        GetInstalledScriptsManager()->GetScriptData(script_url, &script_data);

    // If an error occurred in the browser process while trying to read the
    // installed script, the worker thread will terminate after initialization
    // of the global scope since PrepareForShutdownOnWorkerThread() assumes the
    // global scope has already been initialized.
    switch (status) {
      case InstalledScriptsManager::ScriptStatus::kTaken:
        // InstalledScriptsManager::ScriptStatus::kTaken should not be returned
        // since requesting the main script should be the first and no script
        // has been taken until here.
        NOTREACHED();
      case InstalledScriptsManager::ScriptStatus::kFailed:
        should_terminate = true;
        break;
      case InstalledScriptsManager::ScriptStatus::kSuccess:
        DCHECK(source_code.IsEmpty());
        DCHECK(!cached_meta_data);
        source_code = script_data.TakeSourceText();
        cached_meta_data = script_data.TakeMetaData();

        global_scope_creation_params->content_security_policy_raw_headers =
            script_data.GetContentSecurityPolicyResponseHeaders();
        global_scope_creation_params->referrer_policy =
            script_data.GetReferrerPolicy();
        global_scope_creation_params->origin_trial_tokens =
            script_data.CreateOriginTrialTokens();
        // This may block until CSP and referrer policy are set on the main
        // thread.
        worker_reporting_proxy_.DidLoadInstalledScript(
            global_scope_creation_params->content_security_policy_raw_headers
                .value(),
            global_scope_creation_params->referrer_policy);
        break;
    }
  } else {
    source_code = std::move(global_scope_creation_params->source_code);
    cached_meta_data =
        std::move(global_scope_creation_params->cached_meta_data);
  }

  {
    MutexLocker lock(thread_state_mutex_);

    if (IsOwningBackingThread()) {
      DCHECK(thread_startup_data.has_value());
      GetWorkerBackingThread().InitializeOnBackingThread(*thread_startup_data);
    } else {
      DCHECK(!thread_startup_data.has_value());
    }
    GetWorkerBackingThread().BackingThread().AddTaskObserver(this);

    console_message_storage_ = new ConsoleMessageStorage();
    global_scope_ =
        CreateWorkerGlobalScope(std::move(global_scope_creation_params));
    worker_reporting_proxy_.DidCreateWorkerGlobalScope(GlobalScope());
    worker_inspector_controller_ = WorkerInspectorController::Create(this);

    // TODO(nhiroki): Handle a case where the script controller fails to
    // initialize the context.
    if (GlobalScope()->ScriptController()->InitializeContextIfNeeded(
            String())) {
      worker_reporting_proxy_.DidInitializeWorkerContext();
      v8::HandleScope handle_scope(GetIsolate());
      Platform::Current()->WorkerContextCreated(
          GlobalScope()->ScriptController()->GetContext());
    }

    SetThreadState(lock, ThreadState::kRunning);
  }

  if (start_mode == kPauseWorkerGlobalScopeOnStart)
    StartRunningDebuggerTasksOnPauseOnWorkerThread();

  if (CheckRequestedToTerminateOnWorkerThread() || should_terminate) {
    // Stop further worker tasks from running after this point. WorkerThread
    // was requested to terminate before initialization or during running
    // debugger tasks, or loading the installed main script
    // failed. PerformShutdownOnWorkerThread() will be called soon.
    PrepareForShutdownOnWorkerThread();
    return;
  }

  // TODO(nhiroki): Start module loading for workers here.
  // (https://crbug.com/680046)
  GlobalScope()->EvaluateClassicScript(script_url, std::move(source_code),
                                       std::move(cached_meta_data),
                                       v8_cache_options);
}

void WorkerThread::PrepareForShutdownOnWorkerThread() {
  DCHECK(IsCurrentThread());
  {
    MutexLocker lock(thread_state_mutex_);
    if (thread_state_ == ThreadState::kReadyToShutdown)
      return;
    SetThreadState(lock, ThreadState::kReadyToShutdown);
    if (exit_code_ == ExitCode::kNotTerminated)
      SetExitCode(lock, ExitCode::kGracefullyTerminated);
  }

  inspector_task_runner_->Kill();
  GetWorkerReportingProxy().WillDestroyWorkerGlobalScope();
  probe::AllAsyncTasksCanceled(GlobalScope());

  GlobalScope()->NotifyContextDestroyed();
  if (worker_inspector_controller_) {
    worker_inspector_controller_->Dispose();
    worker_inspector_controller_.Clear();
  }
  global_scope_scheduler_->Dispose();
  GlobalScope()->Dispose();
  global_scope_ = nullptr;

  console_message_storage_.Clear();
  loading_context_.Clear();
  GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);
}

void WorkerThread::PerformShutdownOnWorkerThread() {
  DCHECK(IsCurrentThread());
  DCHECK(CheckRequestedToTerminateOnWorkerThread());
  DCHECK_EQ(ThreadState::kReadyToShutdown, thread_state_);

  if (IsOwningBackingThread())
    GetWorkerBackingThread().ShutdownOnBackingThread();
  // We must not touch workerBackingThread() from now on.

  // Notify the proxy that the WorkerOrWorkletGlobalScope has been disposed
  // of. This can free this thread object, hence it must not be touched
  // afterwards.
  GetWorkerReportingProxy().DidTerminateWorkerThread();

  shutdown_event_->Signal();
}

void WorkerThread::PerformDebuggerTaskOnWorkerThread(CrossThreadClosure task) {
  DCHECK(IsCurrentThread());
  InspectorTaskRunner::IgnoreInterruptsScope scope(
      inspector_task_runner_.get());
  {
    MutexLocker lock(thread_state_mutex_);
    DCHECK_EQ(ThreadState::kRunning, thread_state_);
    running_debugger_task_ = true;
  }
  ThreadDebugger::IdleFinished(GetIsolate());
  {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scoped_us_counter,
        ("WorkerThread.DebuggerTask.Time", 0, 10000000, 50));
    ScopedUsHistogramTimer timer(scoped_us_counter);
    task();
  }
  ThreadDebugger::IdleStarted(GetIsolate());
  {
    MutexLocker lock(thread_state_mutex_);
    running_debugger_task_ = false;
  }
}

void WorkerThread::PerformDebuggerTaskDontWaitOnWorkerThread() {
  DCHECK(IsCurrentThread());
  CrossThreadClosure task = inspector_task_runner_->TakeNextTask(
      InspectorTaskRunner::kDontWaitForTask);
  if (task)
    task();
}

void WorkerThread::SetThreadState(const MutexLocker& lock,
                                  ThreadState next_thread_state) {
  DCHECK(IsThreadStateMutexLocked(lock));
  switch (next_thread_state) {
    case ThreadState::kNotStarted:
      NOTREACHED();
      return;
    case ThreadState::kRunning:
      DCHECK_EQ(ThreadState::kNotStarted, thread_state_);
      thread_state_ = next_thread_state;
      return;
    case ThreadState::kReadyToShutdown:
      DCHECK_EQ(ThreadState::kRunning, thread_state_);
      thread_state_ = next_thread_state;
      return;
  }
}

void WorkerThread::SetExitCode(const MutexLocker& lock, ExitCode exit_code) {
  DCHECK(IsThreadStateMutexLocked(lock));
  DCHECK_EQ(ExitCode::kNotTerminated, exit_code_);
  exit_code_ = exit_code;
}

bool WorkerThread::IsThreadStateMutexLocked(const MutexLocker& /* unused */) {
#if DCHECK_IS_ON()
  // Mutex::locked() is available only if DCHECK_IS_ON() is true.
  return thread_state_mutex_.Locked();
#else
  // Otherwise, believe the given MutexLocker holds |m_threadStateMutex|.
  return true;
#endif
}

bool WorkerThread::CheckRequestedToTerminateOnWorkerThread() {
  MutexLocker lock(thread_state_mutex_);
  return requested_to_terminate_;
}

}  // namespace blink
