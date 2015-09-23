// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameHostScheduler_h
#define WebFrameHostScheduler_h

#include "WebCommon.h"

namespace blink {

class WebFrameScheduler;

class BLINK_PLATFORM_EXPORT WebFrameHostScheduler {
public:
    virtual ~WebFrameHostScheduler() { }

    // The scheduler may throttle tasks associated with background pages.
    virtual void setPageInBackground(bool) { }

    // Creaters a new WebFrameScheduler, the caller is responsible for deleting it.
    virtual WebFrameScheduler* createFrameScheduler() { return nullptr; }
};

} // namespace blink

#endif // WebFrameHostScheduler
