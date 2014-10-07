// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/scheduler/Scheduler.h"

#include "platform/PlatformThreadData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/Task.h"
#include "platform/ThreadTimers.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"

namespace blink {

namespace {

// The time we should stay in CompositorPriority mode for, after a touch event.
double kLowSchedulerPolicyAfterTouchTimeSeconds = 0.1;

} // namespace

// Typically only created from compositor or render threads.
// Note if the scheduler gets shutdown, this may be run after.
class Scheduler::MainThreadPendingHighPriorityTaskRunner : public WebThread::Task {
public:
    MainThreadPendingHighPriorityTaskRunner(
        const Scheduler::Task& task, const TraceLocation& location, const char* traceName)
        : m_task(internal::TracedStandardTask::Create(task, location, traceName))
    {
        ASSERT(Scheduler::shared());
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        m_task->run();
        if (Scheduler* scheduler = Scheduler::shared()) {
            scheduler->updatePolicy();
            scheduler->didRunHighPriorityTask();
        }
    }

private:
    OwnPtr<internal::TracedStandardTask> m_task;
};

// Can be created from any thread.
// Note if the scheduler gets shutdown, this may be run after.
class Scheduler::MainThreadPendingTaskRunner : public WebThread::Task {
public:
    MainThreadPendingTaskRunner(
        const Scheduler::Task& task, const TraceLocation& location, const char* traceName)
        : m_task(internal::TracedStandardTask::Create(task, location, traceName))
    {
        ASSERT(Scheduler::shared());
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        m_task->run();
        if (Scheduler* scheduler = Scheduler::shared()) {
            scheduler->updatePolicy();
        }
    }

private:
    OwnPtr<internal::TracedStandardTask> m_task;
};


// Can be created from any thread.
// Note if the scheduler gets shutdown, this may be run after.
class Scheduler::MainThreadPendingIdleTaskRunner : public WebThread::Task {
public:
    MainThreadPendingIdleTaskRunner()
    {
        ASSERT(Scheduler::shared());
    }

    // WebThread::Task implementation.
    virtual void run() OVERRIDE
    {
        if (Scheduler* scheduler = Scheduler::shared()) {
            scheduler->maybeRunPendingIdleTask();
            // If possible, run the next idle task by reposting on the main thread.
            scheduler->maybePostMainThreadPendingIdleTask();
        }
    }

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
    , m_estimatedNextBeginFrameSeconds(0)
    , m_highPriorityTaskCount(0)
    , m_highPriorityTaskRunnerPosted(false)
    , m_compositorPriorityPolicyEndTimeSeconds(0)
    , m_schedulerPolicy(Normal)
{
}

Scheduler::~Scheduler()
{
}

void Scheduler::willBeginFrame(double estimatedNextBeginFrameSeconds)
{
    ASSERT(isMainThread());
    m_currentFrameCommitted = false;
    m_estimatedNextBeginFrameSeconds = estimatedNextBeginFrameSeconds;
    // TODO: Schedule a deferred task here to run idle work if didCommitFrameToCompositor never gets called.
}

void Scheduler::didCommitFrameToCompositor()
{
    ASSERT(isMainThread());
    m_currentFrameCommitted = true;
    maybePostMainThreadPendingIdleTask();
}

void Scheduler::postHighPriorityTaskInternal(const TraceLocation& location, const Task& task, const char* traceName)
{
    m_mainThread->postTask(new MainThreadPendingHighPriorityTaskRunner(task, location, traceName));
    atomicIncrement(&m_highPriorityTaskCount);
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink.scheduler"), "PendingHighPriorityTasks", m_highPriorityTaskCount);
}

void Scheduler::didRunHighPriorityTask()
{
    atomicDecrement(&m_highPriorityTaskCount);
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink.scheduler"), "PendingHighPriorityTasks", m_highPriorityTaskCount);
}

void Scheduler::postIdleTaskInternal(const TraceLocation& location, const IdleTask& idleTask, const char* traceName)
{
    Locker<Mutex> lock(m_pendingIdleTasksMutex);
    m_pendingIdleTasks.append(internal::TracedIdleTask::Create(idleTask, location, traceName));
}

void Scheduler::postTask(const TraceLocation& location, const Task& task)
{
    m_mainThread->postTask(new MainThreadPendingTaskRunner(task, location, "Scheduler::MainThreadTask"));
}

void Scheduler::postInputTask(const TraceLocation& location, const Task& task)
{
    postHighPriorityTaskInternal(location, task, "Scheduler::InputTask");
}

void Scheduler::didReceiveInputEvent()
{
    // FIXME: We probably want an explicit Disabled policy rather than disabling CompositorPriority.
    if (RuntimeEnabledFeatures::blinkSchedulerEnabled())
        enterSchedulerPolicy(CompositorPriority);
}

void Scheduler::postCompositorTask(const TraceLocation& location, const Task& task)
{
    postHighPriorityTaskInternal(location, task, "Scheduler::CompositorTask");
}

void Scheduler::postIpcTask(const TraceLocation& location, const Task& task)
{
    // FIXME: we want IPCs to be high priority, but we can't currently do that because some of them can take a very long
    // time to process. These need refactoring but we need to add some infrastructure to identify them.
    m_mainThread->postTask(new MainThreadPendingTaskRunner(task, location, "Scheduler::IpcTask"));
}

void Scheduler::postIdleTask(const TraceLocation& location, const IdleTask& idleTask)
{
    postIdleTaskInternal(location, idleTask, "Scheduler::IdleTask");
}

bool Scheduler::maybePostMainThreadPendingIdleTask()
{
    ASSERT(isMainThread());
    TRACE_EVENT0("blink", "Scheduler::maybePostMainThreadPendingIdleTask");
    if (canRunIdleTask()) {
        Locker<Mutex> lock(m_pendingIdleTasksMutex);
        if (!m_pendingIdleTasks.isEmpty()) {
            m_mainThread->postTask(new MainThreadPendingIdleTaskRunner());
            return true;
        }
    }
    return false;
}

void Scheduler::tickSharedTimer()
{
    TRACE_EVENT0("blink", "Scheduler::tickSharedTimer");
    m_sharedTimerFunction();
}

void Scheduler::updatePolicy()
{
    ASSERT(isMainThread());
    Locker<Mutex> lock(m_policyStateMutex);

    // Go back to the normal scheduler policy if enough time has elapsed.
    if (schedulerPolicy() == CompositorPriority && Platform::current()->monotonicallyIncreasingTime() > m_compositorPriorityPolicyEndTimeSeconds)
        enterSchedulerPolicyLocked(Normal);
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
    // It's only worthwhile yielding in CompositorPriority mode.
    if (schedulerPolicy() != CompositorPriority)
        return false;

    // This method is expected to be run on the main thread, but the high priority tasks will be posted by
    // other threads. We could use locks here, but this function is (sometimes) called a lot by
    // ThreadTimers::sharedTimerFiredInternal so we decided to use atomics + barrier loads here which
    // should be cheaper.
    // NOTE it's possible the barrier read is overkill here, since delayed yielding isn't a big deal.
    return acquireLoad(&m_highPriorityTaskCount) != 0;
}

bool Scheduler::maybeRunPendingIdleTask()
{
    ASSERT(isMainThread());
    if (!canRunIdleTask())
        return false;

    takeFirstPendingIdleTask()->run();
    return true;
}

PassOwnPtr<internal::TracedIdleTask> Scheduler::takeFirstPendingIdleTask()
{
    Locker<Mutex> lock(m_pendingIdleTasksMutex);
    ASSERT(!m_pendingIdleTasks.isEmpty());
    return m_pendingIdleTasks.takeFirst();
}

double Scheduler::currentFrameDeadlineForIdleTasks() const
{
    ASSERT(isMainThread());
    // TODO: Make idle time more fine-grain chunks when in Compositor priority.
    return m_estimatedNextBeginFrameSeconds;
}

bool Scheduler::canRunIdleTask() const
{
    ASSERT(isMainThread());
    return m_currentFrameCommitted
        && !shouldYieldForHighPriorityWork()
        && (m_estimatedNextBeginFrameSeconds > Platform::current()->monotonicallyIncreasingTime());
}

Scheduler::SchedulerPolicy Scheduler::schedulerPolicy() const
{
    ASSERT(isMainThread());
    // It's important not to miss the transition from normal to low latency mode, otherwise we're likely to
    // delay the processing of input tasks. Since that transition is triggered by a different thread, we
    // need either a lock or a memory barrier, and the memory barrier is probably cheaper.
    return static_cast<SchedulerPolicy>(acquireLoad(&m_schedulerPolicy));
}

void Scheduler::enterSchedulerPolicy(SchedulerPolicy schedulerPolicy)
{
    Locker<Mutex> lock(m_policyStateMutex);
    enterSchedulerPolicyLocked(schedulerPolicy);
}

void Scheduler::enterSchedulerPolicyLocked(SchedulerPolicy schedulerPolicy)
{
    ASSERT(m_policyStateMutex.locked());
    if (schedulerPolicy == CompositorPriority)
        m_compositorPriorityPolicyEndTimeSeconds = Platform::current()->monotonicallyIncreasingTime() + kLowSchedulerPolicyAfterTouchTimeSeconds;

    releaseStore(&m_schedulerPolicy, schedulerPolicy);
    TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink.scheduler"), "SchedulerPolicy", schedulerPolicy);
}

} // namespace blink
