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

#include "third_party/blink/renderer/core/workers/worker_thread.h"

#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_task_runner.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

using ExitCode = WorkerThread::ExitCode;

namespace {

// TODO(nhiroki): Adjust the delay based on UMA.
constexpr base::TimeDelta kForcibleTerminationDelay =
    base::TimeDelta::FromSeconds(2);

void TerminateThreadsInSet(HashSet<WorkerThread*> threads) {
  for (WorkerThread* thread : threads)
    thread->TerminateForTesting();

  for (WorkerThread* thread : threads)
    thread->WaitForShutdownForTesting();

  // Destruct base::Thread and join the underlying system threads.
  for (WorkerThread* thread : threads)
    thread->ClearWorkerBackingThread();
}

}  // namespace

Mutex& WorkerThread::ThreadSetMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

static std::atomic_int g_unique_worker_thread_id(1);

static int GetNextWorkerThreadId() {
  int next_worker_thread_id =
      g_unique_worker_thread_id.fetch_add(1, std::memory_order_relaxed);
  CHECK_LT(next_worker_thread_id, std::numeric_limits<int>::max());
  return next_worker_thread_id;
}

// RefCountedWaitableEvent makes WaitableEvent thread-safe refcounted.
// WorkerThread retains references to the event from both the parent context
// thread and the worker thread with this wrapper. See
// WorkerThread::PerformShutdownOnWorkerThread() for details.
class WorkerThread::RefCountedWaitableEvent
    : public WTF::ThreadSafeRefCounted<RefCountedWaitableEvent> {
 public:
  static scoped_refptr<RefCountedWaitableEvent> Create() {
    return base::AdoptRef<RefCountedWaitableEvent>(new RefCountedWaitableEvent);
  }

  void Wait() { event_.Wait(); }
  void Signal() { event_.Signal(); }

 private:
  RefCountedWaitableEvent() = default;

  base::WaitableEvent event_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedWaitableEvent);
};

// A class that is passed into V8 Interrupt and via a PostTask. Once both have
// run this object will be destroyed in
// PauseOrFreezeWithInterruptDataOnWorkerThread. The V8 API only takes a raw ptr
// otherwise this could have been done with WTF::Bind and ref counted objects.
class WorkerThread::InterruptData {
 public:
  InterruptData(WorkerThread* worker_thread, mojom::FrameLifecycleState state)
      : worker_thread_(worker_thread), state_(state) {}

  bool ShouldRemoveFromList() { return seen_interrupt_ && seen_post_task_; }
  void MarkPostTaskCalled() { seen_post_task_ = true; }
  void MarkInterruptCalled() { seen_interrupt_ = true; }

  mojom::FrameLifecycleState state() { return state_; }
  WorkerThread* worker_thread() { return worker_thread_; }

 private:
  WorkerThread* worker_thread_;
  mojom::FrameLifecycleState state_;
  bool seen_interrupt_ = false;
  bool seen_post_task_ = false;

  DISALLOW_COPY_AND_ASSIGN(InterruptData);
};

WorkerThread::~WorkerThread() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  MutexLocker lock(ThreadSetMutex());
  DCHECK(InitializingWorkerThreads().Contains(this) ||
         WorkerThreads().Contains(this));
  InitializingWorkerThreads().erase(this);
  WorkerThreads().erase(this);

  DCHECK(child_threads_.IsEmpty());
  DCHECK_NE(ExitCode::kNotTerminated, exit_code_);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, exit_code_histogram,
      ("WorkerThread.ExitCode", static_cast<int>(ExitCode::kLastEnum)));
  exit_code_histogram.Count(static_cast<int>(exit_code_));
}

void WorkerThread::Start(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const base::Optional<WorkerBackingThreadStartupData>& thread_startup_data,
    std::unique_ptr<WorkerDevToolsParams> devtools_params) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  devtools_worker_token_ = devtools_params->devtools_worker_token;

  // Synchronously initialize the per-global-scope scheduler to prevent someone
  // from posting a task to the thread before the scheduler is ready.
  base::WaitableEvent waitable_event;
  PostCrossThreadTask(
      *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::InitializeSchedulerOnWorkerThread,
                          CrossThreadUnretained(this),
                          CrossThreadUnretained(&waitable_event)));
  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    waitable_event.Wait();
  }

  inspector_task_runner_ =
      InspectorTaskRunner::Create(GetTaskRunner(TaskType::kInternalInspector));

  PostCrossThreadTask(
      *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          &WorkerThread::InitializeOnWorkerThread, CrossThreadUnretained(this),
          WTF::Passed(std::move(global_scope_creation_params)),
          thread_startup_data, WTF::Passed(std::move(devtools_params))));
}

void WorkerThread::EvaluateClassicScript(
    const KURL& script_url,
    const String& source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  PostCrossThreadTask(
      *GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::EvaluateClassicScriptOnWorkerThread,
                          CrossThreadUnretained(this), script_url, source_code,
                          WTF::Passed(std::move(cached_meta_data)), stack_id));
}

void WorkerThread::FetchAndRunClassicScript(
    const KURL& script_url,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object_data,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  PostCrossThreadTask(
      *GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(
          &WorkerThread::FetchAndRunClassicScriptOnWorkerThread,
          CrossThreadUnretained(this), script_url,
          WTF::Passed(std::move(outside_settings_object_data)),
          WrapCrossThreadPersistent(outside_resource_timing_notifier),
          stack_id));
}

void WorkerThread::FetchAndRunModuleScript(
    const KURL& script_url,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object_data,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    RejectCoepUnsafeNone reject_coep_unsafe_none) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  PostCrossThreadTask(
      *GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(
          &WorkerThread::FetchAndRunModuleScriptOnWorkerThread,
          CrossThreadUnretained(this), script_url,
          WTF::Passed(std::move(outside_settings_object_data)),
          WrapCrossThreadPersistent(outside_resource_timing_notifier),
          credentials_mode, reject_coep_unsafe_none.value()));
}

void WorkerThread::Pause() {
  PauseOrFreeze(mojom::FrameLifecycleState::kPaused);
}

void WorkerThread::Freeze() {
  PauseOrFreeze(mojom::FrameLifecycleState::kFrozen);
}

void WorkerThread::Resume() {
  // Might be called from any thread.
  if (IsCurrentThread()) {
    ResumeOnWorkerThread();
  } else {
    PostCrossThreadTask(
        *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(&WorkerThread::ResumeOnWorkerThread,
                            CrossThreadUnretained(this)));
  }
}

void WorkerThread::Terminate() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  {
    MutexLocker lock(mutex_);
    if (requested_to_terminate_)
      return;
    requested_to_terminate_ = true;
  }

  // Schedule a task to forcibly terminate the script execution in case that the
  // shutdown sequence does not start on the worker thread in a certain time
  // period.
  ScheduleToTerminateScriptExecution();

  inspector_task_runner_->Dispose();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetWorkerBackingThread().BackingThread().GetTaskRunner();
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::PrepareForShutdownOnWorkerThread,
                          CrossThreadUnretained(this)));
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(&WorkerThread::PerformShutdownOnWorkerThread,
                          CrossThreadUnretained(this)));
}

void WorkerThread::TerminateForTesting() {
  // Schedule a regular async worker thread termination task, and forcibly
  // terminate the V8 script execution to ensure the task runs.
  Terminate();
  EnsureScriptExecutionTerminates(ExitCode::kSyncForciblyTerminated);
}

void WorkerThread::TerminateAllWorkersForTesting() {
  DCHECK(IsMainThread());

  // Keep this lock to prevent WorkerThread instances from being destroyed.
  MutexLocker lock(ThreadSetMutex());
  TerminateThreadsInSet(InitializingWorkerThreads());
  TerminateThreadsInSet(WorkerThreads());
}

void WorkerThread::WillProcessTask(const base::PendingTask& pending_task,
                                   bool was_blocked_or_low_priority) {
  DCHECK(IsCurrentThread());

  // No tasks should get executed after we have closed.
  DCHECK(!GlobalScope()->IsClosing());
}

void WorkerThread::DidProcessTask(const base::PendingTask& pending_task) {
  DCHECK(IsCurrentThread());

  // TODO(tzik): Move this to WorkerThreadScheduler::OnTaskCompleted(), so that
  // metrics for microtasks are counted as a part of the preceding task.
  // TODO(nhiroki): Replace this null check with DCHECK(agent) after making
  // WorkletGlobalScope take a proper Agent.
  if (Agent* agent = GlobalScope()->GetAgent()) {
    agent->event_loop()->PerformMicrotaskCheckpoint();
  }

  // Microtask::PerformCheckpoint() runs microtasks and its completion hooks for
  // the default microtask queue. The default queue may contain the microtasks
  // queued by V8 itself, and legacy blink::MicrotaskQueue::EnqueueMicrotask.
  // The completion hook contains IndexedDB clean-up task, as described at
  // https://html.spec.whatwg.org/C#perform-a-microtask-checkpoint
  // TODO(tzik): Move rejected promise handling to EventLoop.

  GlobalScope()->ScriptController()->GetRejectedPromises()->ProcessQueue();
  if (GlobalScope()->IsClosing()) {
    // This WorkerThread will eventually be requested to terminate.
    GetWorkerReportingProxy().DidCloseWorkerGlobalScope();

    // Stop further worker tasks to run after this point based on the spec:
    // https://html.spec.whatwg.org/C/#close-a-worker
    //
    // "To close a worker, given a workerGlobal, run these steps:"
    // Step 1: "Discard any tasks that have been added to workerGlobal's event
    // loop's task queues."
    // Step 2: "Set workerGlobal's closing flag to true. (This prevents any
    // further tasks from being queued.)"
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

void WorkerThread::DebuggerTaskStarted() {
  MutexLocker lock(mutex_);
  DCHECK(IsCurrentThread());
  debugger_task_counter_++;
}

void WorkerThread::DebuggerTaskFinished() {
  MutexLocker lock(mutex_);
  DCHECK(IsCurrentThread());
  debugger_task_counter_--;
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
  return InitializingWorkerThreads().size() + WorkerThreads().size();
}

HashSet<WorkerThread*>& WorkerThread::InitializingWorkerThreads() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
  return threads;
}

HashSet<WorkerThread*>& WorkerThread::WorkerThreads() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
  return threads;
}

PlatformThreadId WorkerThread::GetPlatformThreadId() {
  return GetWorkerBackingThread().BackingThread().ThreadId();
}

bool WorkerThread::IsForciblyTerminated() {
  MutexLocker lock(mutex_);
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

void WorkerThread::WaitForShutdownForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  base::ScopedAllowBaseSyncPrimitives allow_wait;
  shutdown_event_->Wait();
}

ExitCode WorkerThread::GetExitCodeForTesting() {
  MutexLocker lock(mutex_);
  return exit_code_;
}

scheduler::WorkerScheduler* WorkerThread::GetScheduler() {
  DCHECK(IsCurrentThread());
  return worker_scheduler_.get();
}

void WorkerThread::ChildThreadStartedOnWorkerThread(WorkerThread* child) {
  DCHECK(IsCurrentThread());
#if DCHECK_IS_ON()
  {
    MutexLocker lock(mutex_);
    DCHECK_EQ(ThreadState::kRunning, thread_state_);
  }
#endif
  child_threads_.insert(child);
}

void WorkerThread::ChildThreadTerminatedOnWorkerThread(WorkerThread* child) {
  DCHECK(IsCurrentThread());
  child_threads_.erase(child);
  if (child_threads_.IsEmpty() && CheckRequestedToTerminate())
    PerformShutdownOnWorkerThread();
}

WorkerThread::WorkerThread(WorkerReportingProxy& worker_reporting_proxy)
    : WorkerThread(worker_reporting_proxy, Thread::Current()->GetTaskRunner()) {
}

WorkerThread::WorkerThread(WorkerReportingProxy& worker_reporting_proxy,
                           scoped_refptr<base::SingleThreadTaskRunner>
                               parent_thread_default_task_runner)
    : time_origin_(base::TimeTicks::Now()),
      worker_thread_id_(GetNextWorkerThreadId()),
      forcible_termination_delay_(kForcibleTerminationDelay),
      worker_reporting_proxy_(worker_reporting_proxy),
      parent_thread_default_task_runner_(
          std::move(parent_thread_default_task_runner)),
      shutdown_event_(RefCountedWaitableEvent::Create()) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  MutexLocker lock(ThreadSetMutex());
  InitializingWorkerThreads().insert(this);
}

void WorkerThread::ScheduleToTerminateScriptExecution() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  DCHECK(!forcible_termination_task_handle_.IsActive());
  // It's safe to post a task bound with |this| to the parent thread default
  // task runner because this task is canceled on the destructor of this
  // class on the parent thread.
  forcible_termination_task_handle_ = PostDelayedCancellableTask(
      *parent_thread_default_task_runner_, FROM_HERE,
      WTF::Bind(&WorkerThread::EnsureScriptExecutionTerminates,
                WTF::Unretained(this), ExitCode::kAsyncForciblyTerminated),
      forcible_termination_delay_);
}

WorkerThread::TerminationState WorkerThread::ShouldTerminateScriptExecution() {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  switch (thread_state_) {
    case ThreadState::kNotStarted:
      // Shutdown sequence will surely start during initialization sequence
      // on the worker thread. Don't have to schedule a termination task.
      return TerminationState::kTerminationUnnecessary;
    case ThreadState::kRunning:
      // Terminating during debugger task may lead to crash due to heavy use
      // of v8 api in debugger. Any debugger task is guaranteed to finish, so
      // we can wait for the completion.
      return debugger_task_counter_ > 0 ? TerminationState::kPostponeTerminate
                                        : TerminationState::kTerminate;
    case ThreadState::kReadyToShutdown:
      // Shutdown sequence might have started in a nested event loop but
      // JS might continue running after it exits the nested loop.
      return exit_code_ == ExitCode::kNotTerminated
                 ? TerminationState::kTerminate
                 : TerminationState::kTerminationUnnecessary;
  }
  NOTREACHED();
  return TerminationState::kTerminationUnnecessary;
}

void WorkerThread::EnsureScriptExecutionTerminates(ExitCode exit_code) {
  DCHECK_CALLED_ON_VALID_THREAD(parent_thread_checker_);
  MutexLocker lock(mutex_);
  switch (ShouldTerminateScriptExecution()) {
    case TerminationState::kTerminationUnnecessary:
      return;
    case TerminationState::kTerminate:
      break;
    case TerminationState::kPostponeTerminate:
      ScheduleToTerminateScriptExecution();
      return;
  }

  DCHECK(exit_code == ExitCode::kSyncForciblyTerminated ||
         exit_code == ExitCode::kAsyncForciblyTerminated);
  SetExitCode(exit_code);

  GetIsolate()->TerminateExecution();
  forcible_termination_task_handle_.Cancel();
}

void WorkerThread::InitializeSchedulerOnWorkerThread(
    base::WaitableEvent* waitable_event) {
  DCHECK(IsCurrentThread());
  DCHECK(!worker_scheduler_);
  auto& worker_thread = static_cast<scheduler::WorkerThread&>(
      GetWorkerBackingThread().BackingThread());
  worker_scheduler_ = std::make_unique<scheduler::WorkerScheduler>(
      static_cast<scheduler::WorkerThreadScheduler*>(
          worker_thread.GetNonMainThreadScheduler()),
      worker_thread.worker_scheduler_proxy());
  waitable_event->Signal();
}

void WorkerThread::InitializeOnWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const base::Optional<WorkerBackingThreadStartupData>& thread_startup_data,
    std::unique_ptr<WorkerDevToolsParams> devtools_params) {
  DCHECK(IsCurrentThread());
  worker_reporting_proxy_.WillInitializeWorkerContext();
  {
    TRACE_EVENT0("blink.worker", "WorkerThread::InitializeWorkerContext");
    MutexLocker lock(mutex_);
    DCHECK_EQ(ThreadState::kNotStarted, thread_state_);

    if (IsOwningBackingThread()) {
      DCHECK(thread_startup_data.has_value());
      GetWorkerBackingThread().InitializeOnBackingThread(*thread_startup_data);
    } else {
      DCHECK(!thread_startup_data.has_value());
    }
    GetWorkerBackingThread().BackingThread().AddTaskObserver(this);

    // TODO(crbug.com/866666): Ideally this URL should be the response URL of
    // the worker top-level script, while currently can be the request URL
    // for off-the-main-thread top-level script fetch cases.
    const KURL url_for_debugger = global_scope_creation_params->script_url;

    console_message_storage_ = MakeGarbageCollected<ConsoleMessageStorage>();
    inspector_issue_storage_ = MakeGarbageCollected<InspectorIssueStorage>();
    global_scope_ =
        CreateWorkerGlobalScope(std::move(global_scope_creation_params));
    worker_reporting_proxy_.DidCreateWorkerGlobalScope(GlobalScope());

    worker_inspector_controller_ = WorkerInspectorController::Create(
        this, url_for_debugger, inspector_task_runner_,
        std::move(devtools_params));

    // Since context initialization below may fail, we should notify debugger
    // about the new worker thread separately, so that it can resolve it by id
    // at any moment.
    if (WorkerThreadDebugger* debugger =
            WorkerThreadDebugger::From(GetIsolate()))
      debugger->WorkerThreadCreated(this);

    GlobalScope()->ScriptController()->Initialize(url_for_debugger);
    v8::HandleScope handle_scope(GetIsolate());
    Platform::Current()->WorkerContextCreated(
        GlobalScope()->ScriptController()->GetContext());

    inspector_task_runner_->InitIsolate(GetIsolate());
    SetThreadState(ThreadState::kRunning);
  }

  if (CheckRequestedToTerminate()) {
    // Stop further worker tasks from running after this point. WorkerThread
    // was requested to terminate before initialization.
    // PerformShutdownOnWorkerThread() will be called soon.
    PrepareForShutdownOnWorkerThread();
    return;
  }

  {
    MutexLocker lock(ThreadSetMutex());
    DCHECK(InitializingWorkerThreads().Contains(this));
    DCHECK(!WorkerThreads().Contains(this));
    InitializingWorkerThreads().erase(this);
    WorkerThreads().insert(this);
  }

  // It is important that no code is run on the Isolate between
  // initializing InspectorTaskRunner and pausing on start.
  // Otherwise, InspectorTaskRunner might interrupt isolate execution
  // from another thread and try to resume "pause on start" before
  // we even paused.
  worker_inspector_controller_->WaitForDebuggerIfNeeded();
}

void WorkerThread::EvaluateClassicScriptOnWorkerThread(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  WorkerGlobalScope* global_scope = To<WorkerGlobalScope>(GlobalScope());
  CHECK(global_scope);
  global_scope->EvaluateClassicScript(script_url, std::move(source_code),
                                      std::move(cached_meta_data), stack_id);
}

void WorkerThread::FetchAndRunClassicScriptOnWorkerThread(
    const KURL& script_url,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  if (!outside_resource_timing_notifier) {
    outside_resource_timing_notifier =
        MakeGarbageCollected<NullWorkerResourceTimingNotifier>();
  }
  To<WorkerGlobalScope>(GlobalScope())
      ->FetchAndRunClassicScript(
          script_url,
          *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
              std::move(outside_settings_object)),
          *outside_resource_timing_notifier, stack_id);
}

void WorkerThread::FetchAndRunModuleScriptOnWorkerThread(
    const KURL& script_url,
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        outside_settings_object,
    WorkerResourceTimingNotifier* outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    bool reject_coep_unsafe_none) {
  if (!outside_resource_timing_notifier) {
    outside_resource_timing_notifier =
        MakeGarbageCollected<NullWorkerResourceTimingNotifier>();
  }
  // Worklets have a different code path to import module scripts.
  // TODO(nhiroki): Consider excluding this code path from WorkerThread like
  // Worklets.
  To<WorkerGlobalScope>(GlobalScope())
      ->FetchAndRunModuleScript(
          script_url,
          *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
              std::move(outside_settings_object)),
          *outside_resource_timing_notifier, credentials_mode,
          RejectCoepUnsafeNone(reject_coep_unsafe_none));
}

void WorkerThread::PrepareForShutdownOnWorkerThread() {
  DCHECK(IsCurrentThread());
  {
    MutexLocker lock(mutex_);
    if (thread_state_ == ThreadState::kReadyToShutdown)
      return;
    SetThreadState(ThreadState::kReadyToShutdown);
  }

  if (pause_or_freeze_count_ > 0) {
    DCHECK(nested_runner_);
    pause_or_freeze_count_ = 0;
    nested_runner_->QuitNow();
  }

  if (WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(GetIsolate()))
    debugger->WorkerThreadDestroyed(this);

  GetWorkerReportingProxy().WillDestroyWorkerGlobalScope();

  probe::AllAsyncTasksCanceled(GlobalScope());
  GlobalScope()->NotifyContextDestroyed();
  worker_scheduler_->Dispose();

  // No V8 microtasks should get executed after shutdown is requested.
  GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);

  for (WorkerThread* child : child_threads_)
    child->Terminate();
}

void WorkerThread::PerformShutdownOnWorkerThread() {
  DCHECK(IsCurrentThread());
  {
    MutexLocker lock(mutex_);
    DCHECK(requested_to_terminate_);
    DCHECK_EQ(ThreadState::kReadyToShutdown, thread_state_);
    if (exit_code_ == ExitCode::kNotTerminated)
      SetExitCode(ExitCode::kGracefullyTerminated);
  }

  // When child workers are present, wait for them to shutdown before shutting
  // down this thread. ChildThreadTerminatedOnWorkerThread() is responsible
  // for completing shutdown on the worker thread after the last child shuts
  // down.
  if (!child_threads_.IsEmpty())
    return;

  inspector_task_runner_->Dispose();
  if (worker_inspector_controller_) {
    worker_inspector_controller_->Dispose();
    worker_inspector_controller_.Clear();
  }

  GlobalScope()->Dispose();
  global_scope_ = nullptr;

  console_message_storage_.Clear();
  inspector_issue_storage_.Clear();

  if (IsOwningBackingThread())
    GetWorkerBackingThread().ShutdownOnBackingThread();
  // We must not touch GetWorkerBackingThread() from now on.

  // Keep the reference to the shutdown event in a local variable so that the
  // worker thread can signal it even after calling DidTerminateWorkerThread(),
  // which may destroy |this|.
  scoped_refptr<RefCountedWaitableEvent> shutdown_event = shutdown_event_;

  // Notify the proxy that the WorkerOrWorkletGlobalScope has been disposed
  // of. This can free this thread object, hence it must not be touched
  // afterwards.
  GetWorkerReportingProxy().DidTerminateWorkerThread();

  // This should be signaled at the end because this may induce the main thread
  // to clear the worker backing thread and stop thread execution in the system
  // level.
  shutdown_event->Signal();
}

void WorkerThread::SetThreadState(ThreadState next_thread_state) {
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

void WorkerThread::SetExitCode(ExitCode exit_code) {
  DCHECK_EQ(ExitCode::kNotTerminated, exit_code_);
  exit_code_ = exit_code;
}

bool WorkerThread::CheckRequestedToTerminate() {
  MutexLocker lock(mutex_);
  return requested_to_terminate_;
}

void WorkerThread::PauseOrFreeze(mojom::FrameLifecycleState state) {
  if (IsCurrentThread()) {
    PauseOrFreezeOnWorkerThread(state);
  } else {
    // We send a V8 interrupt to break active JS script execution because
    // workers might not yield. Likewise we might not be in JS and the
    // interrupt might not fire right away, so we post a task as well.
    // Use a token to mitigate both the interrupt and post task firing.
    MutexLocker lock(mutex_);

    InterruptData* interrupt_data = new InterruptData(this, state);
    pending_interrupts_.insert(std::unique_ptr<InterruptData>(interrupt_data));

    if (auto* isolate = GetIsolate()) {
      isolate->RequestInterrupt(&PauseOrFreezeInsideV8InterruptOnWorkerThread,
                                interrupt_data);
    }
    PostCrossThreadTask(
        *GetWorkerBackingThread().BackingThread().GetTaskRunner(), FROM_HERE,
        CrossThreadBindOnce(
            &WorkerThread::PauseOrFreezeInsidePostTaskOnWorkerThread,
            CrossThreadUnretained(interrupt_data)));
  }
}

void WorkerThread::PauseOrFreezeOnWorkerThread(
    mojom::FrameLifecycleState state) {
  DCHECK(IsCurrentThread());
  DCHECK(state == mojom::FrameLifecycleState::kFrozen ||
         state == mojom::FrameLifecycleState::kPaused);
  pause_or_freeze_count_++;
  GlobalScope()->SetLifecycleState(state);
  GlobalScope()->SetDefersLoadingForResourceFetchers(true);

  // If already paused return early.
  if (pause_or_freeze_count_ > 1)
    return;

  std::unique_ptr<scheduler::WorkerScheduler::PauseHandle> pause_handle =
      GetScheduler()->Pause();
  {
    // Since the nested message loop runner needs to be created and destroyed on
    // the same thread we allocate and destroy a new message loop runner each
    // time we pause or freeze. The AutoReset allows a raw ptr to be stored in
    // the worker thread such that the resume/terminate can quit this runner.
    std::unique_ptr<Platform::NestedMessageLoopRunner> nested_runner =
        Platform::Current()->CreateNestedMessageLoopRunner();
    base::AutoReset<Platform::NestedMessageLoopRunner*> nested_runner_autoreset(
        &nested_runner_, nested_runner.get());
    nested_runner->Run();
  }
  GlobalScope()->SetDefersLoadingForResourceFetchers(false);
  GlobalScope()->SetLifecycleState(mojom::FrameLifecycleState::kRunning);
}

void WorkerThread::ResumeOnWorkerThread() {
  DCHECK(IsCurrentThread());
  if (pause_or_freeze_count_ > 0) {
    DCHECK(nested_runner_);
    pause_or_freeze_count_--;
    if (pause_or_freeze_count_ == 0)
      nested_runner_->QuitNow();
  }
}

void WorkerThread::PauseOrFreezeWithInterruptDataOnWorkerThread(
    InterruptData* interrupt_data) {
  DCHECK(IsCurrentThread());
  bool should_execute = false;
  mojom::FrameLifecycleState state;
  {
    MutexLocker lock(mutex_);
    state = interrupt_data->state();
    // If both the V8 interrupt and PostTask have executed we can remove
    // the matching InterruptData from the |pending_interrupts_| as it is
    // no longer used.
    if (interrupt_data->ShouldRemoveFromList()) {
      auto iter = pending_interrupts_.begin();
      while (iter != pending_interrupts_.end()) {
        if (iter->get() == interrupt_data) {
          pending_interrupts_.erase(iter);
          break;
        }
        ++iter;
      }
    } else {
      should_execute = true;
    }
  }

  if (should_execute) {
    PauseOrFreezeOnWorkerThread(state);
  }
}

void WorkerThread::PauseOrFreezeInsideV8InterruptOnWorkerThread(v8::Isolate*,
                                                                void* data) {
  InterruptData* interrupt_data = static_cast<InterruptData*>(data);
  interrupt_data->MarkInterruptCalled();
  interrupt_data->worker_thread()->PauseOrFreezeWithInterruptDataOnWorkerThread(
      interrupt_data);
}

void WorkerThread::PauseOrFreezeInsidePostTaskOnWorkerThread(
    InterruptData* interrupt_data) {
  interrupt_data->MarkPostTaskCalled();
  interrupt_data->worker_thread()->PauseOrFreezeWithInterruptDataOnWorkerThread(
      interrupt_data);
}

}  // namespace blink
