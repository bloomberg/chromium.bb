// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTaskRunner_h
#define WebTaskRunner_h

#include "WebCommon.h"

namespace blink {

class WebTraceLocation;

// The blink representation of a chromium SingleThreadTaskRunner.
class BLINK_PLATFORM_EXPORT WebTaskRunner {
public:
    virtual ~WebTaskRunner() {}

    class BLINK_PLATFORM_EXPORT Task {
    public:
        virtual ~Task() { }
        virtual void run() = 0;
    };

    // Schedule a task to be run on the the associated WebThread.
    // Takes ownership of |Task|. Can be called from any thread.
    virtual void postTask(const WebTraceLocation&, Task*) {}

    // Schedule a task to be run after |delayMs| on the the associated WebThread.
    // Takes ownership of |Task|. Can be called from any thread.
    // TODO(alexclarke): Remove this when possible.
    virtual void postDelayedTask(const WebTraceLocation&, Task*, long long delayMs) {}

    // Schedule a task to be run after |delayMs| on the the associated WebThread.
    // Takes ownership of |Task|. Can be called from any thread.
    virtual void postDelayedTask(const WebTraceLocation&, Task*, double delayMs) {}

    // TODO(alexclarke): Add helpers for posting bound functions as tasks.
};

} // namespace blink

#endif // WebTaskRunner_h
