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
#include "core/origin_trials/OriginTrialContext.h"
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
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <limits.h>

namespace blink {

class WorkerThread::WorkerMicrotaskRunner : public WebThread::TaskObserver {
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
                // |m_workerThread| will eventually be requested to terminate.
                m_workerThread->workerReportingProxy().workerGlobalScopeClosed();

                // Stop further worker tasks to run after this point.
                m_workerThread->prepareForShutdownOnWorkerThread();
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
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::initializeOnWorkerThread, AllowCrossThreadAccess(this), passed(std::move(startupData))));
}

void WorkerThread::terminate()
{
    DCHECK(isMainThread());

    // Prevent the deadlock between GC and an attempt to terminate a thread.
    SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);

    // Protect against this method, initializeOnWorkerThread() or termination
    // via the global scope racing each other.
    MutexLocker lock(m_threadStateMutex);

    // If terminate has already been called, just return.
    if (m_terminated)
        return;
    m_terminated = true;

    // Signal the thread to notify that the thread's stopping.
    if (m_terminationEvent)
        m_terminationEvent->signal();

    // If the worker thread was never initialized, don't start another
    // shutdown, but still wait for the thread to signal when shutdown has
    // completed on initializeOnWorkerThread().
    if (!m_workerGlobalScope)
        return;

    // Determine if we should terminate the isolate so that the task can
    // be handled by thread event loop. If script execution weren't forbidden,
    // a while(1) loop in JS could keep the thread alive forever.
    //
    // (1) |m_readyToShutdown|: It this is set, the worker thread has already
    // noticed that the thread is about to be terminated and the worker global
    // scope is already disposed, so we don't have to explicitly terminate the
    // isolate.
    //
    // (2) |workerScriptCount() == 1|: If other WorkerGlobalScopes are running
    // on the worker thread, we should not terminate the isolate. This condition
    // is not entirely correct because other scripts can be being initialized or
    // terminated simuletaneously. Though this function itself is protected by a
    // mutex, it is possible that |workerScriptCount()| here is not consistent
    // with that in |initialize| and |shutdown|.
    //
    // (3) |m_runningDebuggerTask|: Terminating during debugger task may lead to
    // crash due to heavy use of v8 api in debugger. Any debugger task is
    // guaranteed to finish, so we can wait for the completion.
    bool shouldTerminateIsolate = !m_readyToShutdown && (workerBackingThread().workerScriptCount() == 1) && !m_runningDebuggerTask;

    if (shouldTerminateIsolate) {
        // TODO(yhirano): TerminateExecution should be called more
        // carefully (https://crbug.com/413518).
        m_workerGlobalScope->scriptController()->willScheduleExecutionTermination();
        isolate()->TerminateExecution();
    }

    m_inspectorTaskRunner->kill();
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::prepareForShutdownOnWorkerThread, AllowCrossThreadAccess(this)));
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::performShutdownOnWorkerThread, AllowCrossThreadAccess(this)));
}

void WorkerThread::terminateAndWait()
{
    DCHECK(isMainThread());
    terminate();
    m_shutdownEvent->wait();
}

void WorkerThread::terminateAndWaitForAllWorkers()
{
    DCHECK(isMainThread());

    // Keep this lock to prevent WorkerThread instances from being destroyed.
    MutexLocker lock(threadSetMutex());
    HashSet<WorkerThread*> threads = workerThreads();
    for (WorkerThread* thread : threads)
        thread->terminate();

    for (WorkerThread* thread : threads)
        thread->m_shutdownEvent->wait();
}

v8::Isolate* WorkerThread::isolate()
{
    return workerBackingThread().isolate();
}

bool WorkerThread::isCurrentThread()
{
    return m_started && workerBackingThread().backingThread().isCurrentThread();
}

void WorkerThread::postTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task)
{
    workerBackingThread().backingThread().postTask(location, createWorkerThreadTask(std::move(task), true));
}

void WorkerThread::appendDebuggerTask(std::unique_ptr<CrossThreadClosure> task)
{
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_readyToShutdown)
            return;
    }
    m_inspectorTaskRunner->appendTask(threadSafeBind(&WorkerThread::runDebuggerTaskOnWorkerThread, AllowCrossThreadAccess(this), passed(std::move(task))));
    {
        MutexLocker lock(m_threadStateMutex);
        if (isolate())
            m_inspectorTaskRunner->interruptAndRunAllTasksDontWait(isolate());
    }
    workerBackingThread().backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::runDebuggerTaskDontWaitOnWorkerThread, AllowCrossThreadAccess(this)));
}

void WorkerThread::startRunningDebuggerTasksOnPause()
{
    m_pausedInDebugger = true;
    ThreadDebugger::idleStarted(isolate());
    std::unique_ptr<CrossThreadClosure> task;
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

unsigned WorkerThread::workerThreadCount()
{
    MutexLocker lock(threadSetMutex());
    return workerThreads().size();
}

PlatformThreadId WorkerThread::platformThreadId()
{
    if (!m_started)
        return 0;
    return workerBackingThread().backingThread().platformThread().threadId();
}

WorkerThread::WorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
    : m_inspectorTaskRunner(adoptPtr(new InspectorTaskRunner()))
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_terminationEvent(adoptPtr(new WaitableEvent(
        WaitableEvent::ResetPolicy::Manual,
        WaitableEvent::InitialState::NonSignaled)))
    , m_shutdownEvent(adoptPtr(new WaitableEvent(
        WaitableEvent::ResetPolicy::Manual,
        WaitableEvent::InitialState::NonSignaled)))
{
    MutexLocker lock(threadSetMutex());
    workerThreads().add(this);
}

std::unique_ptr<CrossThreadClosure> WorkerThread::createWorkerThreadTask(std::unique_ptr<ExecutionContextTask> task, bool isInstrumented)
{
    if (isInstrumented)
        isInstrumented = !task->taskNameForInstrumentation().isEmpty();
    if (isInstrumented) {
        DCHECK(isCurrentThread());
        InspectorInstrumentation::asyncTaskScheduled(workerGlobalScope(), "Worker task", task.get());
    }
    return threadSafeBind(&WorkerThread::performTaskOnWorkerThread, AllowCrossThreadAccess(this), passed(std::move(task)), isInstrumented);
}

void WorkerThread::initializeOnWorkerThread(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    KURL scriptURL = startupData->m_scriptURL;
    String sourceCode = startupData->m_sourceCode;
    WorkerThreadStartMode startMode = startupData->m_startMode;
    OwnPtr<Vector<char>> cachedMetaData = std::move(startupData->m_cachedMetaData);
    V8CacheOptions v8CacheOptions = startupData->m_v8CacheOptions;

    {
        MutexLocker lock(m_threadStateMutex);

        // The worker was terminated before the thread had a chance to run.
        if (m_terminated) {
            // Notify the proxy that the WorkerGlobalScope has been disposed of.
            // This can free this thread object, hence it must not be touched
            // afterwards.
            m_workerReportingProxy.workerThreadTerminated();

            // Notify the main thread that it is safe to deallocate our
            // resources.
            m_shutdownEvent->signal();
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
        if (!scriptController->isExecutionForbidden()) {
            scriptController->initializeContextIfNeeded();

            // If Origin Trials have been registered before the V8 context was ready,
            // then inject them into the context now
            ExecutionContext* executionContext = m_workerGlobalScope->getExecutionContext();
            if (executionContext) {
                OriginTrialContext* originTrialContext = OriginTrialContext::from(executionContext);
                if (originTrialContext)
                    originTrialContext->initializePendingFeatures();
            }
        }
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

void WorkerThread::prepareForShutdownOnWorkerThread()
{
    DCHECK(isCurrentThread());
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_readyToShutdown)
            return;
        m_readyToShutdown = true;
    }

    workerReportingProxy().willDestroyWorkerGlobalScope();
    InspectorInstrumentation::allAsyncTasksCanceled(workerGlobalScope());
    workerGlobalScope()->dispose();
    workerBackingThread().backingThread().removeTaskObserver(m_microtaskRunner.get());
}

void WorkerThread::performShutdownOnWorkerThread()
{
    DCHECK(isCurrentThread());
#if DCHECK_IS_ON
    {
        MutexLocker lock(m_threadStateMutex);
        DCHECK(m_terminated);
        DCHECK(m_readyToShutdown);
    }
#endif

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

    m_shutdownEvent->signal();
}

void WorkerThread::performTaskOnWorkerThread(std::unique_ptr<ExecutionContextTask> task, bool isInstrumented)
{
    DCHECK(isCurrentThread());
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_readyToShutdown)
            return;
    }

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

void WorkerThread::runDebuggerTaskOnWorkerThread(std::unique_ptr<CrossThreadClosure> task)
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

        if (!m_terminated)
            return;
        // terminate() was called. Shutdown sequence will start soon.
    }
    // Stop further worker tasks to run after this point.
    prepareForShutdownOnWorkerThread();
}

void WorkerThread::runDebuggerTaskDontWaitOnWorkerThread()
{
    DCHECK(isCurrentThread());
    std::unique_ptr<CrossThreadClosure> task = m_inspectorTaskRunner->takeNextTask(InspectorTaskRunner::DontWaitForTask);
    if (task)
        (*task)();
}

} // namespace blink
