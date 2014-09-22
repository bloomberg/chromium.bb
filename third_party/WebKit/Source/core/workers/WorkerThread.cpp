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

#include "config.h"

#include "core/workers/WorkerThread.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/dom/Microtask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/workers/DedicatedWorkerGlobalScope.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/PlatformThreadData.h"
#include "platform/Task.h"
#include "platform/ThreadTimers.h"
#include "platform/heap/ThreadState.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebWaitableEvent.h"
#include "public/platform/WebWorkerRunLoop.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

#include <utility>

namespace blink {

namespace {
const int64 kShortIdleHandlerDelayMs = 1000;
const int64 kLongIdleHandlerDelayMs = 10*1000;

class MicrotaskRunner : public WebThread::TaskObserver {
public:
    virtual void willProcessTask() OVERRIDE { }
    virtual void didProcessTask() OVERRIDE
    {
        Microtask::performCheckpoint();
    }
};

} // namespace

static Mutex& threadSetMutex()
{
    AtomicallyInitializedStatic(Mutex&, mutex = *new Mutex);
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

class WorkerSharedTimer : public SharedTimer {
public:
    explicit WorkerSharedTimer(WorkerThread* workerThread)
        : m_workerThread(workerThread)
        , m_nextFireTime(0.0)
        , m_running(false)
    { }

    typedef void (*SharedTimerFunction)();
    virtual void setFiredFunction(SharedTimerFunction func)
    {
        m_sharedTimerFunction = func;
        if (!m_sharedTimerFunction)
            m_nextFireTime = 0.0;
    }

    virtual void setFireInterval(double interval)
    {
        ASSERT(m_sharedTimerFunction);

        // See BlinkPlatformImpl::setSharedTimerFireInterval for explanation of
        // why ceil is used in the interval calculation.
        int64 delay = static_cast<int64>(ceil(interval * 1000));

        if (delay < 0) {
            delay = 0;
            m_nextFireTime = 0.0;
        }

        m_running = true;
        m_nextFireTime = currentTime() + interval;
        m_workerThread->postDelayedTask(createSameThreadTask(&WorkerSharedTimer::OnTimeout, this), delay);
    }

    virtual void stop()
    {
        m_running = false;
    }

    double nextFireTime() { return m_nextFireTime; }

private:
    void OnTimeout()
    {
        ASSERT(m_workerThread->workerGlobalScope());
        if (m_sharedTimerFunction && m_running && !m_workerThread->workerGlobalScope()->isClosing())
            m_sharedTimerFunction();
    }

    WorkerThread* m_workerThread;
    SharedTimerFunction m_sharedTimerFunction;
    double m_nextFireTime;
    bool m_running;
};

class WorkerThreadTask : public blink::WebThread::Task {
    WTF_MAKE_NONCOPYABLE(WorkerThreadTask); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<WorkerThreadTask> create(const WorkerThread& workerThread, PassOwnPtr<ExecutionContextTask> task, bool isInstrumented)
    {
        return adoptPtr(new WorkerThreadTask(workerThread, task, isInstrumented));
    }

    virtual ~WorkerThreadTask() { }

    virtual void run() OVERRIDE
    {
        WorkerGlobalScope* workerGlobalScope = m_workerThread.workerGlobalScope();
        // Tasks could be put on the message loop after the cleanup task,
        // ensure none of those are ran.
        if (!workerGlobalScope)
            return;

        if (m_isInstrumented)
            InspectorInstrumentation::willPerformExecutionContextTask(workerGlobalScope, m_task.get());
        if ((!workerGlobalScope->isClosing() && !m_workerThread.terminated()) || m_task->isCleanupTask())
            m_task->performTask(workerGlobalScope);
        if (m_isInstrumented)
            InspectorInstrumentation::didPerformExecutionContextTask(workerGlobalScope);
    }

private:
    WorkerThreadTask(const WorkerThread& workerThread, PassOwnPtr<ExecutionContextTask> task, bool isInstrumented)
        : m_workerThread(workerThread)
        , m_task(task)
        , m_isInstrumented(isInstrumented)
    {
        if (m_isInstrumented)
            m_isInstrumented = !m_task->taskNameForInstrumentation().isEmpty();
        if (m_isInstrumented)
            InspectorInstrumentation::didPostExecutionContextTask(m_workerThread.workerGlobalScope(), m_task.get());
    }

    const WorkerThread& m_workerThread;
    OwnPtr<ExecutionContextTask> m_task;
    bool m_isInstrumented;
};

class RunDebuggerQueueTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<RunDebuggerQueueTask> create(WorkerThread* thread)
    {
        return adoptPtr(new RunDebuggerQueueTask(thread));
    }
    virtual void performTask(ExecutionContext* context) OVERRIDE
    {
        ASSERT(context->isWorkerGlobalScope());
        m_thread->runDebuggerTask(WorkerThread::DontWaitForMessage);
    }

private:
    explicit RunDebuggerQueueTask(WorkerThread* thread) : m_thread(thread) { }

    WorkerThread* m_thread;
};

WorkerThread::WorkerThread(WorkerLoaderProxy& workerLoaderProxy, WorkerReportingProxy& workerReportingProxy, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData> startupData)
    : m_terminated(false)
    , m_workerLoaderProxy(workerLoaderProxy)
    , m_workerReportingProxy(workerReportingProxy)
    , m_startupData(startupData)
    , m_shutdownEvent(adoptPtr(blink::Platform::current()->createWaitableEvent()))
    , m_terminationEvent(adoptPtr(blink::Platform::current()->createWaitableEvent()))
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

void WorkerThread::start()
{
    if (m_thread)
        return;

    m_thread = WebThreadSupportingGC::create("WebCore: Worker");
    m_thread->postTask(new Task(WTF::bind(&WorkerThread::initialize, this)));
}

void WorkerThread::interruptAndDispatchInspectorCommands()
{
    MutexLocker locker(m_workerInspectorControllerMutex);
    if (m_workerInspectorController)
        m_workerInspectorController->interruptAndDispatchInspectorCommands();
}

PlatformThreadId WorkerThread::platformThreadId() const
{
    if (!m_thread)
        return 0;
    return m_thread->platformThread().threadId();
}

void WorkerThread::initialize()
{
    KURL scriptURL = m_startupData->m_scriptURL;
    String sourceCode = m_startupData->m_sourceCode;
    WorkerThreadStartMode startMode = m_startupData->m_startMode;

    {
        MutexLocker lock(m_threadCreationMutex);

        // The worker was terminated before the thread had a chance to run.
        if (m_terminated) {
            // Notify the proxy that the WorkerGlobalScope has been disposed of.
            // This can free this thread object, hence it must not be touched afterwards.
            m_workerReportingProxy.workerThreadTerminated();
            return;
        }

        m_microtaskRunner = adoptPtr(new MicrotaskRunner);
        m_thread->addTaskObserver(m_microtaskRunner.get());
        m_thread->attachGC();
        m_workerGlobalScope = createWorkerGlobalScope(m_startupData.release());

        m_sharedTimer = adoptPtr(new WorkerSharedTimer(this));
        PlatformThreadData::current().threadTimers().setSharedTimer(m_sharedTimer.get());
    }

    // The corresponding call to didStopWorkerRunLoop is in
    // ~WorkerScriptController.
    blink::Platform::current()->didStartWorkerRunLoop(blink::WebWorkerRunLoop(this));

    // Notify proxy that a new WorkerGlobalScope has been created and started.
    m_workerReportingProxy.workerGlobalScopeStarted(m_workerGlobalScope.get());

    WorkerScriptController* script = m_workerGlobalScope->script();
    if (!script->isExecutionForbidden())
        script->initializeContextIfNeeded();
    InspectorInstrumentation::willEvaluateWorkerScript(workerGlobalScope(), startMode);
    script->evaluate(ScriptSourceCode(sourceCode, scriptURL));

    postInitialize();

    postDelayedTask(createSameThreadTask(&WorkerThread::idleHandler, this), kShortIdleHandlerDelayMs);
}

void WorkerThread::cleanup()
{

    // This should be called before we start the shutdown procedure.
    workerReportingProxy().willDestroyWorkerGlobalScope();

    // The below assignment will destroy the context, which will in turn notify messaging proxy.
    // We cannot let any objects survive past thread exit, because no other thread will run GC or otherwise destroy them.
    // If Oilpan is enabled, we detach of the context/global scope, with the final heap cleanup below sweeping it out.
#if !ENABLE(OILPAN)
    ASSERT(m_workerGlobalScope->hasOneRef());
#endif
    m_workerGlobalScope->dispose();
    m_workerGlobalScope = nullptr;

    m_thread->detachGC();

    m_thread->removeTaskObserver(m_microtaskRunner.get());
    m_microtaskRunner = nullptr;

    // Notify the proxy that the WorkerGlobalScope has been disposed of.
    // This can free this thread object, hence it must not be touched afterwards.
    workerReportingProxy().workerThreadTerminated();

    m_terminationEvent->signal();

    // Clean up PlatformThreadData before WTF::WTFThreadData goes away!
    PlatformThreadData::current().destroy();
}

class WorkerThreadShutdownFinishTask : public ExecutionContextTask {
public:
    static PassOwnPtr<WorkerThreadShutdownFinishTask> create()
    {
        return adoptPtr(new WorkerThreadShutdownFinishTask());
    }

    virtual void performTask(ExecutionContext *context)
    {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        workerGlobalScope->clearInspector();
        // It's not safe to call clearScript until all the cleanup tasks posted by functions initiated by WorkerThreadShutdownStartTask have completed.
        workerGlobalScope->clearScript();
        workerGlobalScope->thread()->m_thread->postTask(new Task(WTF::bind(&WorkerThread::cleanup, workerGlobalScope->thread())));
    }

    virtual bool isCleanupTask() const { return true; }
};

class WorkerThreadShutdownStartTask : public ExecutionContextTask {
public:
    static PassOwnPtr<WorkerThreadShutdownStartTask> create()
    {
        return adoptPtr(new WorkerThreadShutdownStartTask());
    }

    virtual void performTask(ExecutionContext *context)
    {
        WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
        workerGlobalScope->stopFetch();
        workerGlobalScope->stopActiveDOMObjects();
        PlatformThreadData::current().threadTimers().setSharedTimer(nullptr);

        // Event listeners would keep DOMWrapperWorld objects alive for too long. Also, they have references to JS objects,
        // which become dangling once Heap is destroyed.
        workerGlobalScope->removeAllEventListeners();

        // Stick a shutdown command at the end of the queue, so that we deal
        // with all the cleanup tasks the databases post first.
        workerGlobalScope->postTask(WorkerThreadShutdownFinishTask::create());
    }

    virtual bool isCleanupTask() const { return true; }
};

void WorkerThread::stop()
{
    // Prevent the deadlock between GC and an attempt to stop a thread.
    ThreadState::SafePointScope safePointScope(ThreadState::HeapPointersOnStack);
    stopInternal();
}

void WorkerThread::stopInShutdownSequence()
{
    stopInternal();
}

void WorkerThread::stopInternal()
{
    // Protect against this method and initialize() racing each other.
    MutexLocker lock(m_threadCreationMutex);

    // If stop has already been called, just return.
    if (m_terminated)
        return;
    m_terminated = true;

    // Signal the thread to notify that the thread's stopping.
    if (m_shutdownEvent)
        m_shutdownEvent->signal();

    if (!m_workerGlobalScope)
        return;

    // Ensure that tasks are being handled by thread event loop. If script execution weren't forbidden, a while(1) loop in JS could keep the thread alive forever.
    m_workerGlobalScope->script()->scheduleExecutionTermination();
    InspectorInstrumentation::didKillAllExecutionContextTasks(m_workerGlobalScope.get());
    m_debuggerMessageQueue.kill();
    postTask(WorkerThreadShutdownStartTask::create());
}

void WorkerThread::terminateAndWaitForAllWorkers()
{
    // Keep this lock to prevent WorkerThread instances from being destroyed.
    MutexLocker lock(threadSetMutex());
    HashSet<WorkerThread*> threads = workerThreads();
    for (HashSet<WorkerThread*>::iterator itr = threads.begin(); itr != threads.end(); ++itr)
        (*itr)->stopInShutdownSequence();

    for (HashSet<WorkerThread*>::iterator itr = threads.begin(); itr != threads.end(); ++itr)
        (*itr)->terminationEvent()->wait();
}

bool WorkerThread::isCurrentThread() const
{
    return m_thread && m_thread->isCurrentThread();
}

void WorkerThread::idleHandler()
{
    ASSERT(m_workerGlobalScope.get());
    int64 delay = kLongIdleHandlerDelayMs;

    // Do a script engine idle notification if the next event is distant enough.
    const double kMinIdleTimespan = 0.3;
    if (m_sharedTimer->nextFireTime() == 0.0 || m_sharedTimer->nextFireTime() > currentTime() + kMinIdleTimespan) {
        bool hasMoreWork = !m_workerGlobalScope->idleNotification();
        if (hasMoreWork)
            delay = kShortIdleHandlerDelayMs;
    }

    postDelayedTask(createSameThreadTask(&WorkerThread::idleHandler, this), delay);
}

void WorkerThread::postTask(PassOwnPtr<ExecutionContextTask> task)
{
    m_thread->postTask(WorkerThreadTask::create(*this, task, true).leakPtr());
}

void WorkerThread::postDelayedTask(PassOwnPtr<ExecutionContextTask> task, long long delayMs)
{
    m_thread->postDelayedTask(WorkerThreadTask::create(*this, task, true).leakPtr(), delayMs);
}

void WorkerThread::postDebuggerTask(PassOwnPtr<ExecutionContextTask> task)
{
    m_debuggerMessageQueue.append(WorkerThreadTask::create(*this, task, false));
    postTask(RunDebuggerQueueTask::create(this));
}

MessageQueueWaitResult WorkerThread::runDebuggerTask(WaitMode waitMode)
{
    ASSERT(isCurrentThread());
    MessageQueueWaitResult result;
    double absoluteTime = MessageQueue<blink::WebThread::Task>::infiniteTime();
    OwnPtr<blink::WebThread::Task> task;
    {
        if (waitMode == DontWaitForMessage)
            absoluteTime = 0.0;
        ThreadState::SafePointScope safePointScope(ThreadState::NoHeapPointersOnStack);
        task = m_debuggerMessageQueue.waitForMessageWithTimeout(result, absoluteTime);
    }

    if (result == MessageQueueMessageReceived) {
        InspectorInstrumentation::willProcessTask(workerGlobalScope());
        task->run();
        InspectorInstrumentation::didProcessTask(workerGlobalScope());
    }

    return result;
}

void WorkerThread::willEnterNestedLoop()
{
    InspectorInstrumentation::willEnterNestedRunLoop(m_workerGlobalScope.get());
}

void WorkerThread::didLeaveNestedLoop()
{
    InspectorInstrumentation::didLeaveNestedRunLoop(m_workerGlobalScope.get());
}

void WorkerThread::setWorkerInspectorController(WorkerInspectorController* workerInspectorController)
{
    MutexLocker locker(m_workerInspectorControllerMutex);
    m_workerInspectorController = workerInspectorController;
}

} // namespace blink
