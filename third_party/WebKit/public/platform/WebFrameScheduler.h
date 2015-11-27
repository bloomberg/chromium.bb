// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameScheduler_h
#define WebFrameScheduler_h

#include "WebCommon.h"

#include <string>

namespace blink {

class WebSecurityOrigin;
class WebTaskRunner;

class BLINK_PLATFORM_EXPORT WebFrameScheduler {
public:
    virtual ~WebFrameScheduler() { }

    // The scheduler may throttle tasks associated with offscreen frames.
    virtual void setFrameVisible(bool) { }

    // Returns the WebTaskRunner for loading tasks.
    // WebFrameScheduler owns the returned WebTaskRunner.
    virtual WebTaskRunner* loadingTaskRunner() { return nullptr; }

    // Returns the WebTaskRunner for timer tasks.
    // WebFrameScheduler owns the returned WebTaskRunner.
    virtual WebTaskRunner* timerTaskRunner() { return nullptr; }

    // Record the current origin. This is for task attribution in tracing.
    virtual void setFrameOrigin(const WebSecurityOrigin&) { }
};

} // namespace blink

#endif // WebFrameScheduler_h
