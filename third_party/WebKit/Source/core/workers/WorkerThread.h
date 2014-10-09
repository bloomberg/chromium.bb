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

#include "core/dom/ExecutionContextTask.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/SharedTimer.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Forward.h"
#include "wtf/MessageQueue.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class KURL;
class WebWaitableEvent;
class WorkerGlobalScope;
class WorkerInspectorController;
class WorkerLoaderProxy;
class WorkerReportingProxy;
class WorkerSharedTimer;
class WorkerThreadShutdownFinishTask;
class WorkerThreadStartupData;
class WorkerThreadTask;

enum WorkerThreadStartMode {
    DontPauseWorkerGlobalScopeOnStart,
    PauseWorkerGlobalScopeOnStart
};

class WorkerThread : public RefCounted<WorkerThread> {
public:
    virtual ~WorkerThread();

    virtual void start();
    virtual void stop();

    // Can be used to wait for this worker thread to shut down.
    // (This is signalled on the main thread, so it's assumed to be waited on the worker context thread)
    WebWaitableEvent* shutdownEvent() { return m_shutdownEvent.get(); }

    WebWaitableEvent* terminationEvent() { return m_terminationEvent.get(); }
    static void terminateAndWaitForAllWorkers();

    bool isCurrentThread() const;
    WorkerLoaderProxy& workerLoaderProxy() const { return m_workerLoaderProxy; }
    WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

    void postTask(PassOwnPtr<ExecutionContextTask>);
    void postDebuggerTask(PassOwnPtr<ExecutionContextTask>);

    enum WaitMode { WaitForMessage, DontWaitForMessage };
    MessageQueueWaitResult runDebuggerTask(WaitMode = WaitForMessage);

    // These methods should be called if the holder of the thread is
    // going to call runDebuggerTask in a loop.
    void willEnterNestedLoop();
    void didLeaveNestedLoop();

    WorkerGlobalScope* workerGlobalScope() const { return m_workerGlobalScope.get(); }
    bool terminated();

    // Number of active worker threads.
    static unsigned workerThreadCount();

    PlatformThreadId platformThreadId() const;

    void interruptAndDispatchInspectorCommands();
    void setWorkerInspectorController(WorkerInspectorController*);

protected:
    WorkerThread(WorkerLoaderProxy&, WorkerReportingProxy&, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>);

    // Factory method for creating a new worker context for the thread.
    virtual PassRefPtrWillBeRawPtr<WorkerGlobalScope> createWorkerGlobalScope(PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>) = 0;

    virtual void postInitialize() { }

private:
    friend class WorkerSharedTimer;
    friend class WorkerThreadShutdownFinishTask;

    void stopInShutdownSequence();
    void stopInternal();

    void initialize();
    void cleanup();
    void idleHandler();
    void postDelayedTask(PassOwnPtr<ExecutionContextTask>, long long delayMs);

    bool m_terminated;
    OwnPtr<WorkerSharedTimer> m_sharedTimer;
    MessageQueue<WorkerThreadTask> m_debuggerMessageQueue;
    OwnPtr<WebThread::TaskObserver> m_microtaskRunner;

    WorkerLoaderProxy& m_workerLoaderProxy;
    WorkerReportingProxy& m_workerReportingProxy;

    RefPtrWillBePersistent<WorkerInspectorController> m_workerInspectorController;
    Mutex m_workerInspectorControllerMutex;

    Mutex m_threadCreationMutex;
    RefPtrWillBePersistent<WorkerGlobalScope> m_workerGlobalScope;
    OwnPtrWillBePersistent<WorkerThreadStartupData> m_startupData;

    // Used to signal thread shutdown.
    OwnPtr<WebWaitableEvent> m_shutdownEvent;

    // Used to signal thread termination.
    OwnPtr<WebWaitableEvent> m_terminationEvent;

    // FIXME: This has to be last because of crbug.com/401397 - the
    // WorkerThread might get deleted before it had a chance to properly
    // shut down. By deleting the WebThread first, we can guarantee that
    // no pending tasks on the thread might want to access any of the other
    // members during the WorkerThread's destruction.
    OwnPtr<WebThreadSupportingGC> m_thread;
};

} // namespace blink

#endif // WorkerThread_h
