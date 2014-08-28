// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/PlatformThreadData.h"
#include "platform/Task.h"
#include "platform/ThreadTimers.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"
#include "wtf/ThreadingPrimitives.h"


namespace blink {

namespace {

// Can be created from any thread.
// Note if the scheduler gets shutdown, this may be run after.
class MainThreadIdleTaskAdapter : public WebThread::Task {
public:
    MainThreadIdleTaskAdapter(const Scheduler::IdleTask& idleTask, double allottedTimeMs, const TraceLocation& location)
        : m_idleTask(idleTask)
        , m_allottedTimeMs(allottedTimeMs)
        , m_location(location)
    {
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        TRACE_EVENT2("blink", "MainThreadIdleTaskAdapter::run",
            "src_file", m_location.fileName(),
            "src_func", m_location.functionName());
        m_idleTask(m_allottedTimeMs);
    }

private:
    Scheduler::IdleTask m_idleTask;
    double m_allottedTimeMs;
    TraceLocation m_location;
};

} // namespace

// Typically only created from compositor or render threads.
// Note if the scheduler gets shutdown, this may be run after.
class Scheduler::MainThreadPendingHighPriorityTaskRunner : public WebThread::Task {
public:
    MainThreadPendingHighPriorityTaskRunner()
    {
        ASSERT(Scheduler::shared());
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        Scheduler* scheduler = Scheduler::shared();
        // FIXME: This check should't be necessary, tasks should not outlive blink.
        ASSERT(scheduler);
        if (!scheduler)
            return;
        scheduler->swapQueuesRunPendingTasksAndAllowHighPriorityTaskRunnerPosting();
    }
};


// Can be created from any thread.
// Note if the scheduler gets shutdown, this may be run after.
class Scheduler::MainThreadPendingTaskRunner : public WebThread::Task {
public:
    MainThreadPendingTaskRunner(
        const Scheduler::Task& task, const TraceLocation& location)
        : m_task(task, location)
    {
        ASSERT(Scheduler::shared());
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        Scheduler* scheduler = Scheduler::shared();
        // FIXME: This check should't be necessary, tasks should not outlive blink.
        ASSERT(scheduler);
        if (scheduler)
            Scheduler::shared()->swapQueuesAndRunPendingTasks();
        m_task.run();
    }

    Scheduler::TracedTask m_task;
};

Scheduler* Scheduler::s_sharedScheduler = nullptr;

void Scheduler::initializeOnMainThread()
{
    s_sharedScheduler = new Scheduler();
}

void Scheduler::shutdown()
{
    delete s_sharedScheduler;
    s_sharedScheduler = nullptr;
}

Scheduler* Scheduler::shared()
{
    return s_sharedScheduler;
}

Scheduler::Scheduler()
    : m_sharedTimerFunction(nullptr)
    , m_mainThread(blink::Platform::current()->currentThread())
    , m_highPriorityTaskCount(0)
    , m_highPriorityTaskRunnerPosted(false)
{
}

Scheduler::~Scheduler()
{
    while (hasPendingHighPriorityWork()) {
        swapQueuesAndRunPendingTasks();
    }
}

void Scheduler::scheduleIdleTask(const TraceLocation& location, const IdleTask& idleTask)
{
    // TODO: send a real allottedTime here.
    m_mainThread->postTask(new MainThreadIdleTaskAdapter(idleTask, 0, location));
}

void Scheduler::postTask(const TraceLocation& location, const Task& task)
{
    m_mainThread->postTask(new MainThreadPendingTaskRunner(task, location));
}

void Scheduler::postInputTask(const TraceLocation& location, const Task& task)
{
    Locker<Mutex> lock(m_pendingTasksMutex);
    m_pendingHighPriorityTasks.append(TracedTask(task, location));
    atomicIncrement(&m_highPriorityTaskCount);
    maybePostMainThreadPendingHighPriorityTaskRunner();
}

void Scheduler::postCompositorTask(const TraceLocation& location, const Task& task)
{
    Locker<Mutex> lock(m_pendingTasksMutex);
    m_pendingHighPriorityTasks.append(TracedTask(task, location));
    atomicIncrement(&m_highPriorityTaskCount);
    maybePostMainThreadPendingHighPriorityTaskRunner();
}

void Scheduler::maybePostMainThreadPendingHighPriorityTaskRunner()
{
    ASSERT(m_pendingTasksMutex.locked());
    if (m_highPriorityTaskRunnerPosted)
        return;
    m_mainThread->postTask(new MainThreadPendingHighPriorityTaskRunner());
    m_highPriorityTaskRunnerPosted = true;
}

void Scheduler::postIdleTask(const TraceLocation& location, const IdleTask& idleTask)
{
    scheduleIdleTask(location, idleTask);
}

void Scheduler::tickSharedTimer()
{
    TRACE_EVENT0("blink", "Scheduler::tickSharedTimer");

    // Run any high priority tasks that are queued up, otherwise the blink timers will yield immediately.
    bool workDone = swapQueuesAndRunPendingTasks();
    m_sharedTimerFunction();

    // The blink timers may have just yielded, so run any high priority tasks that where queued up
    // while the blink timers were executing.
    if (!workDone)
        swapQueuesAndRunPendingTasks();
}

bool Scheduler::swapQueuesAndRunPendingTasks()
{
    ASSERT(isMainThread());

    // These locks guard against another thread posting input or compositor tasks while we swap the buffers.
    // One the buffers have been swapped we can safely access the returned deque without having to lock.
    m_pendingTasksMutex.lock();
    Deque<TracedTask>& highPriorityTasks = m_pendingHighPriorityTasks.swapBuffers();
    m_pendingTasksMutex.unlock();
    return executeHighPriorityTasks(highPriorityTasks);
}

void Scheduler::swapQueuesRunPendingTasksAndAllowHighPriorityTaskRunnerPosting()
{
    ASSERT(isMainThread());

    // These locks guard against another thread posting input or compositor tasks while we swap the buffers.
    // One the buffers have been swapped we can safely access the returned deque without having to lock.
    m_pendingTasksMutex.lock();
    Deque<TracedTask>& highPriorityTasks = m_pendingHighPriorityTasks.swapBuffers();
    m_highPriorityTaskRunnerPosted = false;
    m_pendingTasksMutex.unlock();
    executeHighPriorityTasks(highPriorityTasks);
}

bool Scheduler::executeHighPriorityTasks(Deque<TracedTask>& highPriorityTasks)
{
    TRACE_EVENT0("blink", "Scheduler::executeHighPriorityTasks");
    int highPriorityTasksExecuted = 0;
    while (!highPriorityTasks.isEmpty()) {
        highPriorityTasks.takeFirst().run();
        highPriorityTasksExecuted++;
    }

    int highPriorityTaskCount = atomicSubtract(&m_highPriorityTaskCount, highPriorityTasksExecuted);
    ASSERT_UNUSED(highPriorityTaskCount, highPriorityTaskCount >= 0);
    return highPriorityTasksExecuted > 0;
}

void Scheduler::sharedTimerAdapter()
{
    shared()->tickSharedTimer();
}

void Scheduler::setSharedTimerFiredFunction(void (*function)())
{
    m_sharedTimerFunction = function;
    blink::Platform::current()->setSharedTimerFiredFunction(function ? &Scheduler::sharedTimerAdapter : nullptr);
}

void Scheduler::setSharedTimerFireInterval(double interval)
{
    blink::Platform::current()->setSharedTimerFireInterval(interval);
}

void Scheduler::stopSharedTimer()
{
    blink::Platform::current()->stopSharedTimer();
}

bool Scheduler::shouldYieldForHighPriorityWork() const
{
    return hasPendingHighPriorityWork();
}

bool Scheduler::hasPendingHighPriorityWork() const
{
    // This method is expected to be run on the main thread, but the high priority tasks will be posted by
    // other threads. We could use locks here, but this function is (sometimes) called a lot by
    // ThreadTimers::sharedTimerFiredInternal so we decided to use atomics + barrier loads here which
    // should be cheaper.
    // NOTE it's possible the barrier read is overkill here, since delayed yielding isn't a big deal.
    return acquireLoad(&m_highPriorityTaskCount) != 0;
}

void Scheduler::TracedTask::run()
{
    TRACE_EVENT2("blink", "TracedTask::run",
        "src_file", m_location.fileName(),
        "src_func", m_location.functionName());
    m_task();
}

} // namespace blink
