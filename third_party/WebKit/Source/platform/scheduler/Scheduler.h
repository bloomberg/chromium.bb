// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Scheduler_h
#define Scheduler_h

#include "platform/PlatformExport.h"
#include "platform/scheduler/TracedTask.h"
#include "wtf/DoubleBufferedDeque.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {
class WebThread;
struct WebBeginFrameArgs;

// The scheduler is an opinionated gateway for arranging work to be run on the
// main thread. It decides which tasks get priority over others based on a
// scheduling policy and the overall system state.
class PLATFORM_EXPORT Scheduler {
    WTF_MAKE_NONCOPYABLE(Scheduler);
public:
    typedef Function<void()> Task;
    // An IdleTask is passed an allotted time in CLOCK_MONOTONIC milliseconds and is expected to complete within this timeframe.
    typedef Function<void(double allottedTimeMs)> IdleTask;

    static Scheduler* shared();
    static void initializeOnMainThread();
    static void shutdown();

    // Called to notify about the start of a new frame.
    void willBeginFrame(const WebBeginFrameArgs&);

    // Called to notify that a previously begun frame was committed.
    void didCommitFrameToCompositor();

    // The following entrypoints are used to schedule different types of tasks
    // to be run on the main thread. They can be called from any thread.
    void postInputTask(const TraceLocation&, const Task&);
    void postCompositorTask(const TraceLocation&, const Task&);
    void postIpcTask(const TraceLocation&, const Task&);
    void postTask(const TraceLocation&, const Task&); // For generic (low priority) tasks.
    void postIdleTask(const TraceLocation&, const IdleTask&); // For non-critical tasks which may be reordered relative to other task types.

    // Tells the scheduler that the system received an input event. This causes the scheduler to go into
    // Compositor Priority mode for a short duration.
    void didReceiveInputEvent();

    // Returns true if there is high priority work pending on the main thread
    // and the caller should yield to let the scheduler service that work.
    // Can be called on any thread.
    bool shouldYieldForHighPriorityWork() const;

    // The shared timer can be used to schedule a periodic callback which may
    // get preempted by higher priority work.
    void setSharedTimerFiredFunction(void (*function)());
    void setSharedTimerFireInterval(double);
    void stopSharedTimer();

protected:
    class MainThreadPendingTaskRunner;
    class MainThreadPendingHighPriorityTaskRunner;
    friend class MainThreadPendingTaskRunner;
    friend class MainThreadPendingHighPriorityTaskRunner;

    enum SchedulerPolicy {
        Normal,
        CompositorPriority,
    };

    Scheduler();
    virtual ~Scheduler();

    void scheduleIdleTask(const TraceLocation&, const IdleTask&);
    void postHighPriorityTaskInternal(const TraceLocation&, const Task&, const char* traceName);
    void didRunHighPriorityTask();

    static void sharedTimerAdapter();

    // Start of main thread only members -----------------------------------

    // Only does work in CompositorPriority mode. Returns true if any work was done.
    bool runPendingHighPriorityTasksIfInCompositorPriority();

    // Returns true if any work was done.
    bool executeHighPriorityTasks(Deque<TracedTask>&);

    // Return the current SchedulerPolicy.
    SchedulerPolicy schedulerPolicy() const;

    void updatePolicy();

    void tickSharedTimer();

    void (*m_sharedTimerFunction)();

    // End of main thread only members -------------------------------------

    void enterSchedulerPolicyLocked(SchedulerPolicy);
    void enterSchedulerPolicy(SchedulerPolicy);

    static Scheduler* s_sharedScheduler;

    WebThread* m_mainThread;

    // Declared volatile as it is atomically incremented.
    volatile int m_highPriorityTaskCount;

    bool m_highPriorityTaskRunnerPosted;

    Mutex m_policyStateMutex;
    double m_compositorPriorityPolicyEndTimeSeconds;

    // Don't access m_schedulerPolicy directly, use enterSchedulerPolicyLocked and schedulerPolicy instead.
    volatile int m_schedulerPolicy;
};

} // namespace blink

#endif // Scheduler_h
