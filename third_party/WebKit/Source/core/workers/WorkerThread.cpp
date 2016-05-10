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

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IdleTaskRunner.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/WorkerBackingThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/WebThreadSupportingGC.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <limits.h>

namespace blink {

class WorkerMicrotaskRunner : public WebThread::TaskObserver {
public:
    explicit WorkerMicrotaskRunner(WorkerThread* workerThread)
        : m_workerThread(workerThread)
    {
    }

    void willProcessTask() override
    {
        // No tasks should get executed after we have closed.
        DCHECK(!m_workerThread->workerGlobalScope() || !m_workerThread->workerGlobalScope()->isClosing());
    }

    void didProcessTask() override
    {
        Microtask::performCheckpoint(m_workerThread->isolate());
        if (WorkerGlobalScope* globalScope = m_workerThread->workerGlobalScope()) {
            if (WorkerOrWorkletScriptController* scriptController = globalScope->scriptController())
                scriptController->getRejectedPromises()->processQueue();
            if (globalScope->isClosing()) {
                m_workerThread->workerReportingProxy().workerGlobalScopeClosed();
                m_workerThread->shutdown();
            }
        }
    }

private:
    // Thread owns the microtask runner; reference remains
    // valid for the lifetime of this object.
    WorkerThread* m_workerThread;
};

static Mutex& threadSetMutex()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, new Mutex);
    return mutex;
}

static HashSet<WorkerThread*>& workerThreads()
{
    DEFINE_STATIC_LOCAL(HashSet<WorkerThread*>, threads, ());
    return threads;
}

unsigned WorkerThread::workerThreadCount()
{
    MutexLocker lock(threadSetMutex());
    return workerThreads().size();
}

void WorkerThread::performTask(PassOwnPtr<ExecutionContextTask> task, bool isInstrumented)
{
    DCHECK(isCurrentThread());
    WorkerGlobalScope* globalScope = workerGlobalScope();
    // If the thread is terminated before it had a chance initialize (see
    // WorkerThread::Initialize()), we mustn't run any of the posted tasks.
    if (!globalScope) {
        DCHECK(terminated());
        return;
    }

    InspectorInstrumentation::AsyncTask asyncTask(globalScope, task.get(), isInstrumented);
    task->performTask(globalScope);
}

PassOwnPtr<CrossThreadClosure> WorkerThread::createWorkerThreadTask(PassOwnPtr<ExecutionContextTask> task, bool isInstrumented)
{
    if (isInstrumented)
        isInstrumented = !task->taskNameForInstrumentation().isEmpty();
    if (isInstrumented) {
        DCHECK(isCurrentThread());
        InspectorInstrumentation::asyncTaskScheduled(workerGlobalScope(), "Worker task", task.get());
    }
    return threadSafeBind(&WorkerThread::performTask, AllowCrossThreadAccess(this), passed(std::move(task)), isInstrumented);
}

WorkerThread::WorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
    : m_started(false)
    , m_terminated(false)
    , m_shutdown(false)
    , m_pausedInDebugger(false)
    , m_runningDebuggerTask(false)
    , m_shouldTerminateV8Execution(false)
    , m_inspectorTaskRunner(adoptPtr(new InspectorTaskRunner()))
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_webScheduler(nullptr)
    , m_shutdownEvent(adoptPtr(new WaitableEvent(
        WaitableEvent::ResetPolicy::Manual,
        WaitableEvent::InitialState::NonSignaled)))
    , m_terminationEvent(adoptPtr(new WaitableEvent(
        WaitableEvent::ResetPolicy::Manual,
        WaitableEvent::InitialState::NonSignaled)))
{
    MutexLocker lock(threadSetMutex());
    workerThreads().add(this);
}

WorkerThread::~WorkerThread()
{
    MutexLocker lock(threadSetMutex());
    DCHECK(workerThreads().contains(this));
    workerThreads().remove(this);
}

void WorkerThread::start(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    DCHECK(isMainThread());

    if (m_started)
        return;

    m_started = true;
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::initialize, AllowCrossThreadAccess(this), passed(std::move(startupData))));
}

PlatformThreadId WorkerThread::platformThreadId()
{
    if (!m_started)
        return 0;
    return workerBackingThread().backingThread().platformThread().threadId();
}

void WorkerThread::initialize(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    KURL scriptURL = startupData->m_scriptURL;
    String sourceCode = startupData->m_sourceCode;
    WorkerThreadStartMode startMode = startupData->m_startMode;
    OwnPtr<Vector<char>> cachedMetaData = startupData->m_cachedMetaData.release();
    V8CacheOptions v8CacheOptions = startupData->m_v8CacheOptions;
    m_webScheduler = workerBackingThread().backingThread().platformThread().scheduler();

    {
        MutexLocker lock(m_threadStateMutex);

        // The worker was terminated before the thread had a chance to run.
        if (m_terminated) {
            // Notify the proxy that the WorkerGlobalScope has been disposed of.
            // This can free this thread object, hence it must not be touched afterwards.
            m_workerReportingProxy.workerThreadTerminated();
            // Notify the main thread that it is safe to deallocate our resources.
            m_terminationEvent->signal();
            return;
        }

        workerBackingThread().attach();

        if (shouldAttachThreadDebugger())
            V8PerIsolateData::from(isolate())->setThreadDebugger(adoptPtr(new WorkerThreadDebugger(this, isolate())));
        m_microtaskRunner = adoptPtr(new WorkerMicrotaskRunner(this));
        workerBackingThread().backingThread().addTaskObserver(m_microtaskRunner.get());

        // Optimize for memory usage instead of latency for the worker isolate.
        isolate()->IsolateInBackgroundNotification();
        m_workerGlobalScope = createWorkerGlobalScope(std::move(startupData));
        m_workerGlobalScope->scriptLoaded(sourceCode.length(), cachedMetaData.get() ? cachedMetaData->size() : 0);

        // Notify proxy that a new WorkerGlobalScope has been created and started.
        m_workerReportingProxy.workerGlobalScopeStarted(m_workerGlobalScope.get());

        WorkerOrWorkletScriptController* scriptController = m_workerGlobalScope->scriptController();
        if (!scriptController->isExecutionForbidden())
            scriptController->initializeContextIfNeeded();
    }

    if (startMode == PauseWorkerGlobalScopeOnStart)
        startRunningDebuggerTasksOnPause();

    if (m_workerGlobalScope->scriptController()->isContextInitialized()) {
        m_workerReportingProxy.didInitializeWorkerContext();
        v8::HandleScope handleScope(isolate());
        Platform::current()->workerContextCreated(m_workerGlobalScope->scriptController()->context());
    }

    CachedMetadataHandler* handler = workerGlobalScope()->createWorkerScriptCachedMetadataHandler(scriptURL, cachedMetaData.get());
    bool success = m_workerGlobalScope->scriptController()->evaluate(ScriptSourceCode(sourceCode, scriptURL), nullptr, handler, v8CacheOptions);
    m_workerGlobalScope->didEvaluateWorkerScript();
    m_workerReportingProxy.didEvaluateWorkerScript(success);

    postInitialize();
}

void WorkerThread::shutdown()
{
    DCHECK(isCurrentThread());
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_shutdown)
            return;
        m_shutdown = true;
    }

    // This should be called before we start the shutdown procedure.
    workerReportingProxy().willDestroyWorkerGlobalScope();

    workerGlobalScope()->dispose();

    workerBackingThread().backingThread().removeTaskObserver(m_microtaskRunner.get());
    postTask(BLINK_FROM_HERE, createSameThreadTask(&WorkerThread::performShutdownTask, this));
}

void WorkerThread::performShutdownTask()
{
    DCHECK(isCurrentThread());

    // The below assignment will destroy the context, which will in turn notify
    // messaging proxy. We cannot let any objects survive past thread exit,
    // because no other thread will run GC or otherwise destroy them. If Oilpan
    // is enabled, we detach of the context/global scope, with the final heap
    // cleanup below sweeping it out.
    m_workerGlobalScope->notifyContextDestroyed();
    m_workerGlobalScope = nullptr;

    workerBackingThread().detach();
    // We must not touch workerBackingThread() from now on.

    m_microtaskRunner = nullptr;

    // Notify the proxy that the WorkerGlobalScope has been disposed of.
    // This can free this thread object, hence it must not be touched afterwards.
    workerReportingProxy().workerThreadTerminated();

    m_terminationEvent->signal();
}

void WorkerThread::terminate()
{
    DCHECK(isMainThread());

    // Prevent the deadlock between GC and an attempt to terminate a thread.
    SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);
    terminateInternal();
}

void WorkerThread::terminateAndWait()
{
    DCHECK(isMainThread());
    terminate();
    m_terminationEvent->wait();
}

WorkerGlobalScope* WorkerThread::workerGlobalScope()
{
    DCHECK(isCurrentThread());
    return m_workerGlobalScope.get();
}

bool WorkerThread::terminated()
{
    MutexLocker lock(m_threadStateMutex);
    return m_terminated;
}

void WorkerThread::terminateInternal()
{
    DCHECK(isMainThread());

    // Protect against this method, initialize() or termination via the global
    // scope racing each other.
    MutexLocker lock(m_threadStateMutex);

    // If terminateInternal has already been called, just return.
    if (m_terminated)
        return;
    m_terminated = true;

    // Signal the thread to notify that the thread's stopping.
    if (m_shutdownEvent)
        m_shutdownEvent->signal();

    // If the thread has already initiated shut down, just return.
    if (m_shutdown)
        return;

    // If the worker thread was never initialized, don't start another
    // shutdown, but still wait for the thread to signal when termination has
    // completed.
    if (!m_workerGlobalScope)
        return;

    // Ensure that tasks are being handled by thread event loop. If script
    // execution weren't forbidden, a while(1) loop in JS could keep the thread
    // alive forever.
    m_workerGlobalScope->scriptController()->willScheduleExecutionTermination();

    if (workerBackingThread().workerScriptCount() == 1) {
        // This condition is not entirely correct because other scripts
        // can be being initialized or terminated simuletaneously. Though this
        // function itself is protected by a mutex, it is possible that
        // |workerScriptCount()| here is not consistent with that in
        // |initialize| and |shutdown|.
        // TODO(yhirano): TerminateExecution should be called more carefully.
        // https://crbug.com/413518
        if (m_runningDebuggerTask) {
            // Terminating during debugger task may lead to crash due to heavy
            // use of v8 api in debugger. Any debugger task is guaranteed to
            // finish, so we can postpone termination after task has finished.
            // Note: m_runningDebuggerTask and m_shouldTerminateV8Execution
            // access must be guarded by the lock.
            m_shouldTerminateV8Execution = true;
        } else {
            isolate()->TerminateExecution();
        }
    }

    InspectorInstrumentation::allAsyncTasksCanceled(m_workerGlobalScope.get());
    m_inspectorTaskRunner->kill();
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::shutdown, AllowCrossThreadAccess(this)));
}

v8::Isolate* WorkerThread::isolate()
{
    return workerBackingThread().isolate();
}

void WorkerThread::terminateAndWaitForAllWorkers()
{
    DCHECK(isMainThread());

    // Keep this lock to prevent WorkerThread instances from being destroyed.
    MutexLocker lock(threadSetMutex());
    HashSet<WorkerThread*> threads = workerThreads();
    for (WorkerThread* thread : threads)
        thread->terminateInternal();

    for (WorkerThread* thread : threads)
        thread->m_terminationEvent->wait();
}

bool WorkerThread::isCurrentThread()
{
    return m_started && workerBackingThread().backingThread().isCurrentThread();
}

void WorkerThread::postTask(const WebTraceLocation& location, PassOwnPtr<ExecutionContextTask> task)
{
    workerBackingThread().backingThread().postTask(location, createWorkerThreadTask(std::move(task), true));
}

void WorkerThread::runDebuggerTaskDontWait()
{
    DCHECK(isCurrentThread());
    OwnPtr<CrossThreadClosure> task = m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::DontWaitForTask);
    if (task)
        (*task)();
}

void WorkerThread::appendDebuggerTask(PassOwnPtr<CrossThreadClosure> task)
{
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_shutdown)
            return;
    }
    m_inspectorTaskRunner->appendTask(threadSafeBind(&WorkerThread::runDebuggerTask, AllowCrossThreadAccess(this), passed(std::move(task))));
    {
        MutexLocker lock(m_threadStateMutex);
        if (isolate())
            m_inspectorTaskRunner->interruptAndRunAllTasksDontWait(isolate());
    }
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::runDebuggerTaskDontWait, AllowCrossThreadAccess(this)));
}

void WorkerThread::runDebuggerTask(PassOwnPtr<CrossThreadClosure> task)
{
    DCHECK(isCurrentThread());
    InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
    {
        MutexLocker lock(m_threadStateMutex);
        m_runningDebuggerTask = true;
    }
    ThreadDebugger::idleFinished(isolate());
    (*task)();
    ThreadDebugger::idleStarted(isolate());
    {
        MutexLocker lock(m_threadStateMutex);
        m_runningDebuggerTask = false;
        if (m_shouldTerminateV8Execution) {
            m_shouldTerminateV8Execution = false;
            isolate()->TerminateExecution();
        }
    }
}

void WorkerThread::startRunningDebuggerTasksOnPause()
{
    m_pausedInDebugger = true;
    ThreadDebugger::idleStarted(isolate());
    OwnPtr<CrossThreadClosure> task;
    do {
        {
            SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);
            task = m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::WaitForTask);
        }
        if (task)
            (*task)();
    // Keep waiting until execution is resumed.
    } while (task && m_pausedInDebugger);
    ThreadDebugger::idleFinished(isolate());
}

void WorkerThread::stopRunningDebuggerTasksOnPause()
{
    m_pausedInDebugger = false;
}

} // namespace blink
