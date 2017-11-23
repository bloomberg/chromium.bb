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

#ifndef WorkerThread_h
#define WorkerThread_h

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerBackingThreadStartupData.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadLifecycleContext.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "platform/WaitableEvent.h"
#include "platform/WebTaskRunner.h"
#include "platform/scheduler/child/worker_global_scope_scheduler.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Optional.h"
#include "public/platform/WebThread.h"
#include "v8/include/v8.h"

namespace blink {

class ConsoleMessageStorage;
class InspectorTaskRunner;
class InstalledScriptsManager;
class WorkerBackingThread;
class WorkerInspectorController;
class WorkerOrWorkletGlobalScope;
class WorkerReportingProxy;
struct GlobalScopeCreationParams;
struct GlobalScopeInspectorCreationParams;

// WorkerThread is a kind of WorkerBackingThread client. Each worker mechanism
// can access the lower thread infrastructure via an implementation of this
// abstract class. Multiple WorkerThreads may share one WorkerBackingThread for
// worklets.
//
// WorkerThread start and termination must be initiated on the main thread and
// an actual task is executed on the worker thread.
//
// When termination starts, (debugger) tasks on WorkerThread are handled as
// follows:
//  - A running task may finish unless a forcible termination task interrupts.
//    If the running task is for debugger, it's guaranteed to finish without
//    any interruptions.
//  - Queued tasks never run.
class CORE_EXPORT WorkerThread : public WebThread::TaskObserver {
 public:
  // Represents how this thread is terminated. Used for UMA. Append only.
  enum class ExitCode {
    kNotTerminated,
    kGracefullyTerminated,
    kSyncForciblyTerminated,
    kAsyncForciblyTerminated,
    kLastEnum,
  };

  ~WorkerThread() override;

  // Starts the underlying thread and creates the global scope. Called on the
  // main thread.
  // Startup data for WorkerBackingThread must be WTF::nullopt if |this| doesn't
  // own the underlying WorkerBackingThread.
  // TODO(nhiroki): We could separate WorkerBackingThread initialization from
  // GlobalScope initialization sequence, that is, InitializeOnWorkerThread().
  // After that, we could remove this startup data for WorkerBackingThread.
  // (https://crbug.com/710364)
  void Start(std::unique_ptr<GlobalScopeCreationParams>,
             const WTF::Optional<WorkerBackingThreadStartupData>&,
             std::unique_ptr<GlobalScopeInspectorCreationParams>,
             ParentFrameTaskRunners*);

  // Closes the global scope and terminates the underlying thread. Called on the
  // main thread.
  void Terminate();

  // Called on the main thread for the leak detector. Forcibly terminates the
  // script execution and waits by *blocking* the calling thread until the
  // workers are shut down. Please be careful when using this function, because
  // after the synchronous termination any V8 APIs may suddenly start to return
  // empty handles and it may cause crashes.
  static void TerminateAllWorkersForTesting();

  // WebThread::TaskObserver.
  void WillProcessTask() override;
  void DidProcessTask() override;

  virtual WorkerBackingThread& GetWorkerBackingThread() = 0;
  virtual void ClearWorkerBackingThread() = 0;
  ConsoleMessageStorage* GetConsoleMessageStorage() const {
    return console_message_storage_.Get();
  }
  v8::Isolate* GetIsolate();

  bool IsCurrentThread();

  // Called on the worker thread.
  ThreadableLoadingContext* GetLoadingContext();

  WorkerReportingProxy& GetWorkerReportingProxy() const {
    return worker_reporting_proxy_;
  }

  void AppendDebuggerTask(CrossThreadClosure);

  // Runs only debugger tasks while paused in debugger.
  void StartRunningDebuggerTasksOnPauseOnWorkerThread();
  void StopRunningDebuggerTasksOnPauseOnWorkerThread();

  // Can be called only on the worker thread, WorkerOrWorkletGlobalScope
  // and WorkerInspectorController are not thread safe.
  WorkerOrWorkletGlobalScope* GlobalScope();
  WorkerInspectorController* GetWorkerInspectorController();

  // Called for creating WorkerThreadLifecycleObserver on both the main thread
  // and the worker thread.
  WorkerThreadLifecycleContext* GetWorkerThreadLifecycleContext() const {
    return worker_thread_lifecycle_context_;
  }

  // Number of active worker threads.
  static unsigned WorkerThreadCount();

  // Returns a set of all worker threads. This must be called only on the main
  // thread and the returned set must not be stored for future use.
  static HashSet<WorkerThread*>& WorkerThreads();

  int GetWorkerThreadId() const { return worker_thread_id_; }

  PlatformThreadId GetPlatformThreadId();

  bool IsForciblyTerminated();

  void WaitForShutdownForTesting() { shutdown_event_->Wait(); }
  ExitCode GetExitCodeForTesting();

  ParentFrameTaskRunners* GetParentFrameTaskRunners() const {
    return parent_frame_task_runners_.Get();
  }

  // For ServiceWorkerScriptStreaming. Returns nullptr otherwise.
  virtual InstalledScriptsManager* GetInstalledScriptsManager() {
    return nullptr;
  }

  // Can be called on both the main thread and the worker thread.
  scoped_refptr<WebTaskRunner> GetTaskRunner(TaskType type) {
    return global_scope_scheduler_->GetTaskRunner(type);
  }

 protected:
  WorkerThread(ThreadableLoadingContext*, WorkerReportingProxy&);

  // Official moment of creation of worker: when the worker thread is created.
  // (https://w3c.github.io/hr-time/#time-origin)
  const double time_origin_;

 private:
  friend class WorkerThreadTest;
  FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest, ShouldTerminateScriptExecution);
  FRIEND_TEST_ALL_PREFIXES(
      WorkerThreadTest,
      Terminate_WhileDebuggerTaskIsRunningOnInitialization);
  FRIEND_TEST_ALL_PREFIXES(WorkerThreadTest,
                           Terminate_WhileDebuggerTaskIsRunning);

  // Represents the state of this worker thread. A caller may need to acquire
  // a lock |m_threadStateMutex| before accessing this:
  //   - Only the worker thread can set this with the lock.
  //   - The worker thread can read this without the lock.
  //   - The main thread can read this with the lock.
  enum class ThreadState {
    kNotStarted,
    kRunning,
    kReadyToShutdown,
  };

  // Factory method for creating a new worker context for the thread.
  // Called on the worker thread.
  virtual WorkerOrWorkletGlobalScope* CreateWorkerGlobalScope(
      std::unique_ptr<GlobalScopeCreationParams>) = 0;

  // Returns true when this WorkerThread owns the associated
  // WorkerBackingThread exclusively. If this function returns true, the
  // WorkerThread initializes / shutdowns the backing thread. Otherwise
  // the backing thread should be initialized / shutdown properly out of this
  // class.
  virtual bool IsOwningBackingThread() const { return true; }

  // Posts a delayed task to forcibly terminate script execution in case the
  // normal shutdown sequence does not start within a certain time period.
  void ScheduleToTerminateScriptExecution();

  // Returns true if we should synchronously terminate the script execution so
  // that a shutdown task can be handled by the thread event loop. This must be
  // called with |m_threadStateMutex| acquired.
  bool ShouldTerminateScriptExecution(const MutexLocker&);

  // Terminates worker script execution if the worker thread is running and not
  // already shutting down. Does not terminate if a debugger task is running,
  // because the debugger task is guaranteed to finish and it heavily uses V8
  // API calls which would crash after forcible script termination. Called on
  // the main thread.
  void EnsureScriptExecutionTerminates(ExitCode);

  // These are called in this order during worker thread startup.
  void InitializeSchedulerOnWorkerThread(WaitableEvent*);
  void InitializeOnWorkerThread(
      std::unique_ptr<GlobalScopeCreationParams>,
      const WTF::Optional<WorkerBackingThreadStartupData>&,
      std::unique_ptr<GlobalScopeInspectorCreationParams>);

  // These are called in this order during worker thread termination.
  void PrepareForShutdownOnWorkerThread();
  void PerformShutdownOnWorkerThread();

  void PerformDebuggerTaskOnWorkerThread(CrossThreadClosure);
  void PerformDebuggerTaskDontWaitOnWorkerThread();

  // These must be called with |m_threadStateMutex| acquired.
  void SetThreadState(const MutexLocker&, ThreadState);
  void SetExitCode(const MutexLocker&, ExitCode);
  bool IsThreadStateMutexLocked(const MutexLocker&);

  // This internally acquires |m_threadStateMutex|. If you already have the
  // lock or you're on the main thread, you should consider directly accessing
  // |m_requestedToTerminate|.
  bool CheckRequestedToTerminateOnWorkerThread();

  // A unique identifier among all WorkerThreads.
  const int worker_thread_id_;

  // Set on the main thread and checked on both the main and worker threads.
  bool requested_to_terminate_ = false;

  // Accessed only on the worker thread.
  bool paused_in_debugger_ = false;

  // Set on the worker thread and checked on both the main and worker threads.
  bool running_debugger_task_ = false;

  ThreadState thread_state_ = ThreadState::kNotStarted;
  ExitCode exit_code_ = ExitCode::kNotTerminated;

  TimeDelta forcible_termination_delay_;

  std::unique_ptr<InspectorTaskRunner> inspector_task_runner_;

  // Created on the main thread, passed to the worker thread but should kept
  // being accessed only on the main thread.
  CrossThreadPersistent<ThreadableLoadingContext> loading_context_;

  WorkerReportingProxy& worker_reporting_proxy_;

  CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;

  // Tasks managed by this scheduler are canceled when the global scope is
  // closed.
  std::unique_ptr<scheduler::WorkerGlobalScopeScheduler>
      global_scope_scheduler_;

  // This lock protects |m_globalScope|, |m_requestedToTerminate|,
  // |m_threadState|, |m_runningDebuggerTask| and |m_exitCode|.
  Mutex thread_state_mutex_;

  CrossThreadPersistent<ConsoleMessageStorage> console_message_storage_;
  CrossThreadPersistent<WorkerOrWorkletGlobalScope> global_scope_;
  CrossThreadPersistent<WorkerInspectorController> worker_inspector_controller_;

  // Signaled when the thread completes termination on the worker thread.
  std::unique_ptr<WaitableEvent> shutdown_event_;

  // Used to cancel a scheduled forcible termination task. See
  // mayForciblyTerminateExecution() for details.
  TaskHandle forcible_termination_task_handle_;

  // Created on the main thread heap, but will be accessed cross-thread
  // when worker thread posts tasks.
  CrossThreadPersistent<WorkerThreadLifecycleContext>
      worker_thread_lifecycle_context_;
};

}  // namespace blink

#endif  // WorkerThread_h
