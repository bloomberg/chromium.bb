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
class WorkerMicrotaskRunner;
class WorkerReportingProxy;
class WorkerThreadStartupData;

enum WorkerThreadStartMode {
    DontPauseWorkerGlobalScopeOnStart,
    PauseWorkerGlobalScopeOnStart
};

// TODO(sadrul): Rename to WorkerScript.
class CORE_EXPORT WorkerThread {
public:
    virtual ~WorkerThread();

    // Called on the main thread.
    void start(PassOwnPtr<WorkerThreadStartupData>);
    void terminate();

    virtual WorkerBackingThread& workerBackingThread() = 0;
    virtual bool shouldAttachThreadDebugger() const { return true; }
    v8::Isolate* isolate();

    // Can be used to wait for this worker thread to shut down.
    // (This is signaled on the main thread, so it's assumed to be waited on
    // the worker context thread)
    WaitableEvent* shutdownEvent() { return m_shutdownEvent.get(); }

    // Called in shutdown sequence on the main thread. Internally calls
    // terminate() (or terminateInternal) and wait (by *blocking* the calling
    // thread) until the worker(s) is/are shut down.
    void terminateAndWait();
    static void terminateAndWaitForAllWorkers();

    bool isCurrentThread();
    WorkerLoaderProxy* workerLoaderProxy() const
    {
        RELEASE_ASSERT(m_workerLoaderProxy);
        return m_workerLoaderProxy.get();
    }

    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    void postTask(const WebTraceLocation&, PassOwnPtr<ExecutionContextTask>);
    void appendDebuggerTask(PassOwnPtr<CrossThreadClosure>);

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

protected:
    WorkerThread(PassRefPtr<WorkerLoaderProxy>, WorkerReportingProxy&);

    // Factory method for creating a new worker context for the thread.
    // Called on the worker thread.
    virtual WorkerGlobalScope* createWorkerGlobalScope(PassOwnPtr<WorkerThreadStartupData>) = 0;

    // Called on the worker thread.
    virtual void postInitialize() { }

private:
    friend class WorkerMicrotaskRunner;

    PassOwnPtr<CrossThreadClosure> createWorkerThreadTask(PassOwnPtr<ExecutionContextTask>, bool isInstrumented);

    // Called on the main thread.
    void terminateInternal();

    // Called on the worker thread.
    void initialize(PassOwnPtr<WorkerThreadStartupData>);
    void shutdown();
    void performTask(PassOwnPtr<ExecutionContextTask>, bool isInstrumented);
    void performShutdownTask();
    void runDebuggerTask(PassOwnPtr<CrossThreadClosure>);
    void runDebuggerTaskDontWait();

    bool m_started;
    bool m_terminated;
    bool m_shutdown;
    bool m_pausedInDebugger;
    bool m_runningDebuggerTask;
    bool m_shouldTerminateV8Execution;
    OwnPtr<InspectorTaskRunner> m_inspectorTaskRunner;
    OwnPtr<WebThread::TaskObserver> m_microtaskRunner;

    RefPtr<WorkerLoaderProxy> m_workerLoaderProxy;
    WorkerReportingProxy& m_workerReportingProxy;
    WebScheduler* m_webScheduler; // Not owned.

    // This lock protects |m_workerGlobalScope|, |m_terminated|, |m_shutdown|,
    // |m_runningDebuggerTask|, |m_shouldTerminateV8Execution| and
    // |m_microtaskRunner|.
    Mutex m_threadStateMutex;

    Persistent<WorkerGlobalScope> m_workerGlobalScope;

    // Used to signal thread shutdown.
    OwnPtr<WaitableEvent> m_shutdownEvent;

    // Used to signal thread termination.
    OwnPtr<WaitableEvent> m_terminationEvent;
};

} // namespace blink

#endif // WorkerThread_h
