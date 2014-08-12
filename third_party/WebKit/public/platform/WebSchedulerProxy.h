// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSchedulerProxy_h
#define WebSchedulerProxy_h

#include "WebThread.h"

namespace blink {

class Scheduler;
class WebTraceLocation;

// This class is used to submit tasks to Blink's main thread scheduler.
class WebSchedulerProxy {
public:
    BLINK_PLATFORM_EXPORT ~WebSchedulerProxy();

    BLINK_PLATFORM_EXPORT static WebSchedulerProxy create();

    // Schedule an input processing task to be run on the Blink main thread.
    // Takes ownership of |Task|. Can be called from any thread.
    BLINK_PLATFORM_EXPORT void postInputTask(const WebTraceLocation&, WebThread::Task*);

    // Schedule a compositor task to run on the Blink main thread. Takes
    // ownership of |Task|. Can be called from any thread.
    BLINK_PLATFORM_EXPORT void postCompositorTask(const WebTraceLocation&, WebThread::Task*);

private:
    WebSchedulerProxy();

    Scheduler* m_scheduler;
};

} // namespace blink

#endif // WebSchedulerProxy_h
