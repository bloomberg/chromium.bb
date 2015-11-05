// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebViewScheduler_h
#define WebViewScheduler_h

#include "WebCommon.h"
#include "public/platform/WebPassOwnPtr.h"

namespace blink {

class WebFrameScheduler;

class BLINK_PLATFORM_EXPORT WebViewScheduler {
public:
    virtual ~WebViewScheduler() { }

    // The scheduler may throttle tasks associated with background pages.
    virtual void setPageInBackground(bool) = 0;

    // Creaters a new WebFrameScheduler, the caller is responsible for deleting it.
    virtual WebPassOwnPtr<WebFrameScheduler> createFrameScheduler() = 0;
};

} // namespace blink

#endif // WebViewScheduler
