// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Scheduler_h
#define Scheduler_h

#include "platform/PlatformExport.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"

namespace blink {
class WebThread;
}

namespace blink {

class TraceLocation;

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

    // The following entrypoints are used to schedule different types of tasks
    // to be run on the main thread. They can be called from any thread.
    void postInputTask(const TraceLocation&, const Task&);
    void postCompositorTask(const TraceLocation&, const Task&);
    void postTask(const TraceLocation&, const Task&); // For generic (low priority) tasks.
    void postIdleTask(const IdleTask&); // For non-critical tasks which may be reordered relative to other task types.

    // Returns true if there is high priority work pending on the main thread
    // and the caller should yield to let the scheduler service that work.
    // Can be called on the main thread.
    bool shouldYieldForHighPriorityWork();

    // The shared timer can be used to schedule a periodic callback which may
    // get preempted by higher priority work.
    void setSharedTimerFiredFunction(void (*function)());
    void setSharedTimerFireInterval(double);
    void stopSharedTimer();

private:
    Scheduler();
    ~Scheduler();

    void scheduleTask(const TraceLocation&, const Task&);
    void scheduleIdleTask(const IdleTask&);

    static void sharedTimerAdapter();
    void tickSharedTimer();

    static Scheduler* s_sharedScheduler;
    blink::WebThread* m_mainThread;

    void (*m_sharedTimerFunction)();
};

} // namespace blink

#endif // Scheduler_h
