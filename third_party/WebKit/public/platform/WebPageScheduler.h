// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPageScheduler_h
#define WebPageScheduler_h

#include "WebCommon.h"

namespace blink {

class WebFrameScheduler;

class BLINK_PLATFORM_EXPORT WebPageScheduler {
public:
    virtual ~WebPageScheduler() { }

    // The scheduler may throttle tasks associated with backgound pages.
    virtual void setPageInBackground(bool) { }

    // Creaters a new WebFrameScheduler, the caller is responsible for deleting it.
    WebFrameScheduler* createFrameScheduler() { return nullptr; }
};

} // namespace blink

#endif // WebPageScheduler_h
