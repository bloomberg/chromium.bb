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

#include "core/CoreExport.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "wtf/Forward.h"
#include "wtf/Functional.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include <v8.h>

namespace blink {

class InspectorTaskRunner;
class WaitableEvent;
class WorkerBackingThread;
class WorkerGlobalScope;
class WorkerInspectorController;
class WorkerReportingProxy;
class WorkerThreadStartupData;

enum WorkerThreadStartMode {
    DontPauseWorkerGlobalScopeOnStart,
    PauseWorkerGlobalScopeOnStart
};

// WorkerThread is a kind of WorkerBackingThread client. Each worker mechanism
// can access the lower thread infrastructure via an implementation of this
// abstract class. Multiple WorkerThreads can share one WorkerBackingThread.
// See WorkerBackingThread.h for more details.
//
// WorkerThread start and termination must be initiated on the main thread and
// an actual task is executed on the worker thread.
class CORE_EXPORT WorkerThread {
public:
    // Represents how this thread is terminated.
    enum class ExitCode {
        NotTerminated,
        GracefullyTerminated,
        SyncForciblyTerminated,
        AsyncForciblyTerminated,
    };

    virtual ~WorkerThread();

    // Called on the main thread.
    void start(PassOwnPtr<WorkerThreadStartupData>);
    void terminate();

    // Called on the main thread. Internally calls terminateInternal() and wait
    // (by *blocking* the calling thread) until the worker(s) is/are shut down.
    void terminateAndWait();
    static void terminateAndWaitForAllWorkers();

    virtual WorkerBackingThread& workerBackingThread() = 0;
    virtual bool shouldAttachThreadDebugger() const { return true; }
    v8::Isolate* isolate();

    // Can be used to wait for this worker thread to terminate.
    // (This is signaled on the main thread, so it's assumed to be waited on
    // the worker context thread)
    WaitableEvent* terminationEvent() { return m_terminationEvent.get(); }

    bool isCurrentThread();

    WorkerLoaderProxy* workerLoaderProxy() const
    {
        RELEASE_ASSERT(m_workerLoaderProxy);
        return m_workerLoaderProxy.get();
    }

    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    void postTask(const WebTraceLocation&, std::unique_ptr<ExecutionContextTask>);
    void appendDebuggerTask(std::unique_ptr<CrossThreadClosure>);

    // Runs only debugger tasks while paused in debugger, called on the worker
    // thread.
    void startRunningDebuggerTasksOnPause();
    void stopRunningDebuggerTasksOnPause();
    bool isRunningDebuggerTasksOnPause() const { return m_pausedInDebugger; }

    // Can be called only on the worker thread, WorkerGlobalScope is not thread
    // safe.
    WorkerGlobalScope* workerGlobalScope();

    // Returns true once one of the terminate* methods is called.
    bool terminated();

    // Number of active worker threads.
    static unsigned workerThreadCount();

    PlatformThreadId platformThreadId();

    ExitCode getExitCode();

protected:
    WorkerThread(PassRefPtr<WorkerLoaderProxy>, WorkerReportingProxy&);

    // Factory method for creating a new worker context for the thread.
    // Called on the worker thread.
    virtual WorkerGlobalScope* createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData>) = 0;

    // Called on the worker thread.
    virtual void postInitialize() { }

private:
    friend class WorkerThreadTest;

    class ForceTerminationTask;
    class WorkerMicrotaskRunner;

    enum class TerminationMode {
        // Synchronously terminate the worker execution. Please be careful to
        // use this mode, because after the synchronous termination any V8 APIs
        // may suddenly start to return empty handles and it may cause crashes.
        Forcible,

        // Don't synchronously terminate the worker execution. Instead, schedule
        // a task to terminate it in case that the shutdown sequence does not
        // start on the worker thread in a certain time period.
        Graceful,
    };

    std::unique_ptr<CrossThreadClosure> createWorkerThreadTask(std::unique_ptr<ExecutionContextTask>, bool isInstrumented);

    void terminateInternal(TerminationMode);
    void forciblyTerminateExecution();

    void initializeOnWorkerThread(PassOwnPtr<WorkerThreadStartupData>);
    void prepareForShutdownOnWorkerThread();
    void performShutdownOnWorkerThread();
    void performTaskOnWorkerThread(std::unique_ptr<ExecutionContextTask>, bool isInstrumented);
    void runDebuggerTaskOnWorkerThread(std::unique_ptr<CrossThreadClosure>);
    void runDebuggerTaskDontWaitOnWorkerThread();

    void setForceTerminationDelayInMsForTesting(long long forceTerminationDelayInMs) { m_forceTerminationDelayInMs = forceTerminationDelayInMs; }

    bool m_started = false;
    bool m_terminated = false;
    bool m_readyToShutdown = false;
    bool m_pausedInDebugger = false;
    bool m_runningDebuggerTask = false;
    ExitCode m_exitCode = ExitCode::NotTerminated;

    long long m_forceTerminationDelayInMs;

    OwnPtr<InspectorTaskRunner> m_inspectorTaskRunner;
    OwnPtr<WorkerMicrotaskRunner> m_microtaskRunner;

    RefPtr<WorkerLoaderProxy> m_workerLoaderProxy;
    WorkerReportingProxy& m_workerReportingProxy;

    // This lock protects |m_workerGlobalScope|, |m_terminated|,
    // |m_readyToShutdown|, |m_runningDebuggerTask|, |m_exitCode| and
    // |m_microtaskRunner|.
    Mutex m_threadStateMutex;

    Persistent<WorkerGlobalScope> m_workerGlobalScope;

    // Signaled when the thread starts termination on the main thread.
    OwnPtr<WaitableEvent> m_terminationEvent;

    // Signaled when the thread completes termination on the worker thread.
    OwnPtr<WaitableEvent> m_shutdownEvent;

    // Scheduled when termination starts with TerminationMode::Force, and
    // cancelled when the worker thread is gracefully shut down.
    OwnPtr<ForceTerminationTask> m_scheduledForceTerminationTask;
};

} // namespace blink

#endif // WorkerThread_h
