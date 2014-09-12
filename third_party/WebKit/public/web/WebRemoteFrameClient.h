// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameClient_h
#define WebRemoteFrameClient_h

#include "public/web/WebDOMMessageEvent.h"
#include "public/web/WebSecurityOrigin.h"

namespace blink {
class WebLocalFrame;
class WebRemoteFrame;

class WebRemoteFrameClient {
public:
    // Notifies the embedder that a postMessage was issued to a remote frame.
    virtual void postMessageEvent(
        WebLocalFrame* sourceFrame,
        WebRemoteFrame* targetFrame,
        WebSecurityOrigin targetOrigin,
        WebDOMMessageEvent) = 0;
};

} // namespace blink

#endif // WebRemoteFrameClient_h
