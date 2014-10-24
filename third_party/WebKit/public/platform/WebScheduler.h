// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebScheduler_h
#define WebScheduler_h

namespace blink {

class WebTraceLocation;

// This class is used to submit tasks and pass other information from Blink to
// the platform's scheduler.
class WebScheduler {
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

    // Schedule an idle task to run the Blink main thread. For non-critical
    // tasks which may be reordered relative to other task types and may be
    // starved for an arbitrarily long time if no idle time is available.
    // Takes ownership of |IdleTask|. Can be called from any thread.
    virtual void postIdleTask(const WebTraceLocation&, IdleTask*) { }
};

} // namespace blink

#endif // WebScheduler_h
