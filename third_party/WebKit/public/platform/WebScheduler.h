// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScheduler_h
#define WebScheduler_h

#include "WebCommon.h"
#include "public/platform/WebThread.h"

namespace blink {

class WebTraceLocation;

// This class is used to submit tasks and pass other information from Blink to
// the platform's scheduler.
class BLINK_PLATFORM_EXPORT WebScheduler {
public:
    virtual ~WebScheduler() { }

    // An IdleTask is passed a deadline in CLOCK_MONOTONIC seconds and is
    // expected to complete before this deadline.
    class IdleTask {
    public:
        virtual ~IdleTask() { }
        virtual void run(double deadlineSeconds) = 0;
    };

    // Called during Blink's shutdown to delete any pending tasks from the
    // scheduler. Must be called on the main thread.
    virtual void shutdown() { }

    // Returns true if there is high priority work pending on the main thread
    // and the caller should yield to let the scheduler service that work.
    // Must be called on the main thread.
    virtual bool shouldYieldForHighPriorityWork() { return false; }

    // Returns true if a currently running idle task could exceed its deadline
    // without impacting user experience too much. This should only be used if
    // there is a task which cannot be pre-empted and is likely to take longer
    // than the largest expected idle task deadline. It should NOT be polled to
    // check whether more work can be performed on the current idle task after
    // its deadline has expired - post a new idle task for the continuation of
    // the work in this case.
    // Must be called from the main thread.
    virtual bool canExceedIdleDeadlineIfRequired() { return false; }

    // Schedule an idle task to run the Blink main thread. For non-critical
    // tasks which may be reordered relative to other task types and may be
    // starved for an arbitrarily long time if no idle time is available.
    // Takes ownership of |IdleTask|. Can be called from any thread.
    virtual void postIdleTask(const WebTraceLocation&, IdleTask*) { }

    // Like postIdleTask but guarantees that the posted task will not run
    // nested within an already-running task. Posting an idle task as
    // non-nestable may not affect when the task gets run, or it could
    // make it run later than it normally would, but it won't make it
    // run earlier than it normally would.
    virtual void postNonNestableIdleTask(const WebTraceLocation&, IdleTask*) { }

    // Like postIdleTask but does not run the idle task until after some other
    // task has run. This enables posting of a task which won't stop the Blink
    // main thread from sleeping, but will start running after it wakes up.
    // Takes ownership of |IdleTask|. Can be called from any thread.
    virtual void postIdleTaskAfterWakeup(const WebTraceLocation&, IdleTask*) { }

    // Schedule a loading task to be run on the Blink main thread. Loading
    // tasks usually have the default priority, but may be deprioritised
    // when the user is interacting with the device.
    // Takes ownership of |WebThread::Task|. Can be called from any thread.
    virtual void postLoadingTask(const WebTraceLocation&, WebThread::Task*) { }
};

} // namespace blink

#endif // WebScheduler_h
