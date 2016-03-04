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

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IdleTaskRunner.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/dom/Microtask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/SafePoint.h"
#include "platform/heap/ThreadState.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/WeakPtr.h"
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
        WorkerGlobalScope* globalScope = m_workerThread->workerGlobalScope();
        ASSERT_UNUSED(globalScope, !globalScope || !globalScope->isClosing());
    }

    void didProcessTask() override
    {
        Microtask::performCheckpoint(m_workerThread->isolate());
        if (WorkerGlobalScope* globalScope = m_workerThread->workerGlobalScope()) {
            if (WorkerOrWorkletScriptController* scriptController = globalScope->scriptController())
                scriptController->rejectedPromises()->processQueue();
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
    ASSERT(isCurrentThread());
    WorkerGlobalScope* globalScope = workerGlobalScope();
    // If the thread is terminated before it had a chance initialize (see
    // WorkerThread::Initialize()), we mustn't run any of the posted tasks.
    if (!globalScope) {
        ASSERT(terminated());
        return;
    }

    if (isInstrumented)
        InspectorInstrumentation::willPerformExecutionContextTask(globalScope, task.get());
    task->performTask(globalScope);
    if (isInstrumented)
        InspectorInstrumentation::didPerformExecutionContextTask(globalScope);
}

PassOwnPtr<Closure> WorkerThread::createWorkerThreadTask(PassOwnPtr<ExecutionContextTask> task, bool isInstrumented)
{
    if (isInstrumented)
        isInstrumented = !task->taskNameForInstrumentation().isEmpty();
    if (isInstrumented) {
        // TODO(hiroshige): This doesn't work when called on the main thread.
        // https://crbug.com/588497
        InspectorInstrumentation::didPostExecutionContextTask(workerGlobalScope(), task.get());
    }
    return threadSafeBind(&WorkerThread::performTask, AllowCrossThreadAccess(this), task, isInstrumented);
}

class WorkerThread::DebuggerTaskQueue {
    WTF_MAKE_NONCOPYABLE(DebuggerTaskQueue);
public:
    using Task = WTF::Closure;
    using Result = WorkerThread::TaskQueueResult;

    DebuggerTaskQueue() { }

    // Returns true if the queue is still alive, false if the queue has been
    // killed.
    bool append(PassOwnPtr<Task> task)
    {
        MutexLocker lock(m_mutex);
        m_queue.append(task);
        m_condition.signal();
        return !m_killed;
    }

    PassOwnPtr<Task> waitWithTimeout(Result& result, double absoluteTime)
    {
        MutexLocker lock(m_mutex);
        bool timedOut = false;

        while (!m_killed && !timedOut && m_queue.isEmpty())
            timedOut = !m_condition.timedWait(m_mutex, absoluteTime);

        ASSERT(!timedOut || absoluteTime != infiniteTime());

        if (m_killed) {
            result = Terminated;
            return nullptr;
        }

        if (timedOut) {
            result = Timeout;
            return nullptr;
        }

        ASSERT_WITH_SECURITY_IMPLICATION(!m_queue.isEmpty());
        result = TaskReceived;

        return m_queue.takeFirst().release();
    }

    void kill()
    {
        MutexLocker lock(m_mutex);
        m_killed = true;
        m_condition.broadcast();
    }

    static double infiniteTime() { return std::numeric_limits<double>::max(); }

private:
    Mutex m_mutex;
    ThreadCondition m_condition;
    Deque<OwnPtr<Task>> m_queue;
    bool m_killed = false;
};

class WorkerThread::RunInspectorCommandsTask : public InspectorTaskRunner::Task {
    WTF_MAKE_NONCOPYABLE(RunInspectorCommandsTask);
public:
    explicit RunInspectorCommandsTask(WorkerThread* thread) : m_thread(thread) { }
    ~RunInspectorCommandsTask() override { }
    void run() override
    {
        // Process all queued debugger commands. WorkerThread is certainly
        // alive if this task is being executed.
        InspectorInstrumentation::willEnterNestedRunLoop(m_thread->workerGlobalScope());
        while (WorkerThread::TaskReceived == m_thread->runDebuggerTask(WorkerThread::DontWaitForTask)) { }
        InspectorInstrumentation::didLeaveNestedRunLoop(m_thread->workerGlobalScope());
    }

private:
    WorkerThread* m_thread;
};

WorkerThread::WorkerThread(PassRefPtr<WorkerLoaderProxy> workerLoaderProxy, WorkerReportingProxy& workerReportingProxy)
    : m_started(false)
    , m_terminated(false)
    , m_shutdown(false)
    , m_pausedInDebugger(false)
    , m_runningDebuggerTask(false)
    , m_shouldTerminateV8Execution(false)
    , m_debuggerTaskQueue(adoptPtr(new DebuggerTaskQueue))
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_webScheduler(nullptr)
    , m_isolate(nullptr)
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
    ASSERT(workerThreads().contains(this));
    workerThreads().remove(this);
}

void WorkerThread::start(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    ASSERT(isMainThread());

    if (m_started)
        return;

    m_started = true;
    backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::initialize, AllowCrossThreadAccess(this), startupData));
}

PlatformThreadId WorkerThread::platformThreadId()
{
    if (!m_started)
        return 0;
    return backingThread().platformThread().threadId();
}

void WorkerThread::initialize(PassOwnPtr<WorkerThreadStartupData> startupData)
{
    KURL scriptURL = startupData->m_scriptURL;
    String sourceCode = startupData->m_sourceCode;
    WorkerThreadStartMode startMode = startupData->m_startMode;
    OwnPtr<Vector<char>> cachedMetaData = startupData->m_cachedMetaData.release();
    V8CacheOptions v8CacheOptions = startupData->m_v8CacheOptions;
    m_webScheduler = backingThread().platformThread().scheduler();

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

        m_microtaskRunner = adoptPtr(new WorkerMicrotaskRunner(this));
        initializeBackingThread();
        backingThread().addTaskObserver(m_microtaskRunner.get());

        m_isolate = initializeIsolate();
        // Optimize for memory usage instead of latency for the worker isolate.
        m_isolate->IsolateInBackgroundNotification();
        m_inspectorTaskRunner = adoptPtr(new InspectorTaskRunner(m_isolate));
        m_workerGlobalScope = createWorkerGlobalScope(startupData);
        m_workerGlobalScope->scriptLoaded(sourceCode.length(), cachedMetaData.get() ? cachedMetaData->size() : 0);

        didStartWorkerThread();

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
    }

    OwnPtrWillBeRawPtr<CachedMetadataHandler> handler(workerGlobalScope()->createWorkerScriptCachedMetadataHandler(scriptURL, cachedMetaData.get()));
    bool success = m_workerGlobalScope->scriptController()->evaluate(ScriptSourceCode(sourceCode, scriptURL), nullptr, handler.get(), v8CacheOptions);
    m_workerGlobalScope->didEvaluateWorkerScript();
    m_workerReportingProxy.didEvaluateWorkerScript(success);

    postInitialize();
}

void WorkerThread::shutdown()
{
    ASSERT(isCurrentThread());
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_shutdown)
            return;
        m_shutdown = true;
    }

    // This should be called before we start the shutdown procedure.
    workerReportingProxy().willDestroyWorkerGlobalScope();

    workerGlobalScope()->dispose();

    // This should be called after the WorkerGlobalScope's disposed (which may
    // trigger some last-minutes cleanups) and before the thread actually stops.
    willStopWorkerThread();

    backingThread().removeTaskObserver(m_microtaskRunner.get());
    postTask(BLINK_FROM_HERE, createSameThreadTask(&WorkerThread::performShutdownTask, this));
}

void WorkerThread::performShutdownTask()
{
    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    // If Oilpan is enabled, we detach of the context/global scope, with the final heap cleanup below sweeping it out.
#if !ENABLE(OILPAN)
    ASSERT(m_workerGlobalScope->hasOneRef());
#endif
    m_workerGlobalScope->notifyContextDestroyed();
    m_workerGlobalScope = nullptr;

    willDestroyIsolate();
    shutdownBackingThread();
    destroyIsolate();
    m_isolate = nullptr;

    m_microtaskRunner = nullptr;

    // Notify the proxy that the WorkerGlobalScope has been disposed of.
    // This can free this thread object, hence it must not be touched afterwards.
    workerReportingProxy().workerThreadTerminated();

    m_terminationEvent->signal();
}

void WorkerThread::terminate()
{
    // Prevent the deadlock between GC and an attempt to terminate a thread.
    SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);
    terminateInternal();
}

void WorkerThread::terminateAndWait()
{
    terminate();
    m_terminationEvent->wait();
}

WorkerGlobalScope* WorkerThread::workerGlobalScope()
{
    ASSERT(isCurrentThread());
    return m_workerGlobalScope.get();
}

bool WorkerThread::terminated()
{
    MutexLocker lock(m_threadStateMutex);
    return m_terminated;
}

void WorkerThread::terminateInternal()
{
    ASSERT(isMainThread());

    // Protect against this method, initialize() or termination via the global scope racing each other.
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

    // Ensure that tasks are being handled by thread event loop. If script execution weren't forbidden, a while(1) loop in JS could keep the thread alive forever.
    m_workerGlobalScope->scriptController()->willScheduleExecutionTermination();

    // Terminating during debugger task may lead to crash due to heavy use of v8 api in debugger.
    // Any debugger task is guaranteed to finish, so we can postpone termination after task has finished.
    // Note: m_runningDebuggerTask and m_shouldTerminateV8Execution access must be guarded by the lock.
    if (m_runningDebuggerTask)
        m_shouldTerminateV8Execution = true;
    else
        terminateV8Execution();

    InspectorInstrumentation::didKillAllExecutionContextTasks(m_workerGlobalScope.get());
    m_debuggerTaskQueue->kill();
    backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::shutdown, AllowCrossThreadAccess(this)));
}

void WorkerThread::didStartWorkerThread()
{
    ASSERT(isCurrentThread());
    Platform::current()->didStartWorkerThread();
}

void WorkerThread::willStopWorkerThread()
{
    ASSERT(isCurrentThread());
    Platform::current()->willStopWorkerThread();
}

void WorkerThread::terminateAndWaitForAllWorkers()
{
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
    return m_started && backingThread().isCurrentThread();
}

void WorkerThread::postTask(const WebTraceLocation& location, PassOwnPtr<ExecutionContextTask> task)
{
    backingThread().postTask(location, createWorkerThreadTask(task, true));
}

void WorkerThread::postDelayedTask(const WebTraceLocation& location, PassOwnPtr<ExecutionContextTask> task, long long delayMs)
{
    backingThread().postDelayedTask(location, createWorkerThreadTask(task, true), delayMs);
}

void WorkerThread::initializeBackingThread()
{
    ASSERT(isCurrentThread());
    backingThread().initialize();
}

void WorkerThread::shutdownBackingThread()
{
    ASSERT(isCurrentThread());
    backingThread().shutdown();
}

v8::Isolate* WorkerThread::initializeIsolate()
{
    ASSERT(isCurrentThread());
    ASSERT(!m_isolate);
    v8::Isolate* isolate = V8PerIsolateData::initialize();
    V8Initializer::initializeWorker(isolate);

    OwnPtr<V8IsolateInterruptor> interruptor = adoptPtr(new V8IsolateInterruptor(isolate));
    ThreadState::current()->addInterruptor(interruptor.release());
    ThreadState::current()->registerTraceDOMWrappers(isolate, V8GCController::traceDOMWrappers);
    if (RuntimeEnabledFeatures::v8IdleTasksEnabled())
        V8PerIsolateData::enableIdleTasks(isolate, adoptPtr(new V8IdleTaskRunner(m_webScheduler)));
    return isolate;
}

void WorkerThread::willDestroyIsolate()
{
    ASSERT(isCurrentThread());
    ASSERT(m_isolate);
    V8PerIsolateData::willBeDestroyed(m_isolate);
}

void WorkerThread::destroyIsolate()
{
    ASSERT(isCurrentThread());
    V8PerIsolateData::destroy(m_isolate);
}

void WorkerThread::terminateV8Execution()
{
    m_isolate->TerminateExecution();
}

void WorkerThread::runDebuggerTaskDontWait()
{
    runDebuggerTask(WorkerThread::DontWaitForTask);
}

void WorkerThread::appendDebuggerTask(PassOwnPtr<Closure> task)
{
    {
        MutexLocker lock(m_threadStateMutex);
        if (m_shutdown)
            return;
    }
    m_debuggerTaskQueue->append(task);
    backingThread().postTask(BLINK_FROM_HERE, threadSafeBind(&WorkerThread::runDebuggerTaskDontWait, AllowCrossThreadAccess(this)));
    if (m_inspectorTaskRunner)
        m_inspectorTaskRunner->interruptAndRun(adoptPtr(new RunInspectorCommandsTask(this)));
}

WorkerThread::TaskQueueResult WorkerThread::runDebuggerTask(WaitMode waitMode)
{
    ASSERT(isCurrentThread());
    TaskQueueResult result;
    double absoluteTime = DebuggerTaskQueue::infiniteTime();
    OwnPtr<Closure> task;
    {
        if (waitMode == DontWaitForTask)
            absoluteTime = 0.0;
        SafePointScope safePointScope(BlinkGC::HeapPointersOnStack);
        task = m_debuggerTaskQueue->waitWithTimeout(result, absoluteTime);
    }

    if (result == TaskReceived) {
        InspectorTaskRunner::IgnoreInterruptsScope scope(m_inspectorTaskRunner.get());
        {
            MutexLocker lock(m_threadStateMutex);
            m_runningDebuggerTask = true;
        }
        InspectorInstrumentation::willProcessTask(workerGlobalScope());
        (*task)();
        InspectorInstrumentation::didProcessTask(workerGlobalScope());
        {
            MutexLocker lock(m_threadStateMutex);
            m_runningDebuggerTask = false;
            if (m_shouldTerminateV8Execution) {
                m_shouldTerminateV8Execution = false;
                terminateV8Execution();
            }
        }
    }

    return result;
}

void WorkerThread::startRunningDebuggerTasksOnPause()
{
    m_pausedInDebugger = true;
    WorkerThread::TaskQueueResult result;
    InspectorInstrumentation::willEnterNestedRunLoop(m_workerGlobalScope.get());
    do {
        result = runDebuggerTask(WorkerThread::WaitForTask);
    // Keep waiting until execution is resumed.
    } while (result == WorkerThread::TaskReceived && m_pausedInDebugger);
    InspectorInstrumentation::didLeaveNestedRunLoop(m_workerGlobalScope.get());
}

void WorkerThread::stopRunningDebuggerTasksOnPause()
{
    m_pausedInDebugger = false;
}

} // namespace blink
