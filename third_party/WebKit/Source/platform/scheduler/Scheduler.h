// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Scheduler_h
#define Scheduler_h

#include "platform/PlatformExport.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class WebScheduler;

// The scheduler is an opinionated gateway for arranging work to be run on the
// main thread. It decides which tasks get priority over others based on a
// scheduling policy and the overall system state.
class PLATFORM_EXPORT Scheduler {
    WTF_MAKE_NONCOPYABLE(Scheduler);
public:
    // An IdleTask is passed a deadline in CLOCK_MONOTONIC seconds and is expected to complete before this deadline.
    typedef Function<void(double deadlineSeconds)> IdleTask;

    static Scheduler* shared();
    static void setForTesting(Scheduler*);
    static void shutdown();

    // For non-critical tasks which may be reordered relative to other task types and may be starved
    // for an arbitrarily long time if no idle time is available.
    void postIdleTask(const WebTraceLocation&, PassOwnPtr<IdleTask>);

    // Like postIdleTask but guarantees that the posted task will not run
    // nested within an already-running task. Posting an idle task as
    // non-nestable may not affect when the task gets run, or it could
    // make it run later than it normally would, but it won't make it
    // run earlier than it normally would.
    void postNonNestableIdleTask(const WebTraceLocation&, PassOwnPtr<IdleTask>);

    // Like postIdleTask but does not run the idle task until after some other
    // task has run. This enables posting of a task which won't stop the Blink
    // main thread from sleeping, but will start running after it wakes up.
    void postIdleTaskAfterWakeup(const WebTraceLocation&, PassOwnPtr<IdleTask>);

    // For tasks related to loading, e.g. HTML parsing.  Loading tasks usually have default priority
    // but they may be deprioritized when the user is interacting with the device.
    // Takes ownership of |WebThread::Task|.
    void postLoadingTask(const WebTraceLocation&, WebThread::Task*);

    // Returns true if there is high priority work pending on the main thread
    // and the caller should yield to let the scheduler service that work.
    // Must be called on the main thread.
    bool shouldYieldForHighPriorityWork() const;

    // Returns true if a currently running idle task could exceed its deadline
    // without impacting user experience too much. This should only be used if
    // there is a task which cannot be pre-empted and is likely to take longer
    // than the largest expected idle task deadline. It should NOT be polled to
    // check whether more work can be performed on the current idle task after
    // its deadline has expired - post a new idle task for the continuation of
    // the work in this case.
    // Must be called from the main thread.
    bool canExceedIdleDeadlineIfRequired() const;

protected:
    Scheduler(WebScheduler*);
    virtual ~Scheduler();

    static Scheduler* s_sharedScheduler;
    WebScheduler* m_webScheduler;
};

} // namespace blink

#endif // Scheduler_h
