// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaSession_h
#define WebMediaSession_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/mediasession/WebMediaSessionError.h"

namespace blink {

using WebMediaSessionActivateCallback = WebCallbacks<void, const WebMediaSessionError&>;
using WebMediaSessionDeactivateCallback = WebCallbacks<void, void>;

struct WebMediaMetadata;

class WebMediaSession {
public:
    virtual ~WebMediaSession() = default;

    virtual void activate(WebMediaSessionActivateCallback*) = 0;
    virtual void deactivate(WebMediaSessionDeactivateCallback*) = 0;

    // Updates the metadata associated with the WebMediaSession. The metadata
    // can be a nullptr in which case the associated metadata should be reset.
    // The pointer is not owned by the WebMediaSession implementation.
    virtual void setMetadata(const WebMediaMetadata*) = 0;
};

} // namespace blink

#endif // WebMediaSession_h
