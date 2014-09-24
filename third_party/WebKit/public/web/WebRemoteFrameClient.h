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
    // Notify the embedder that it should remove this frame from the frame tree
    // and release any resources associated with it.
    virtual void frameDetached() { }

    // Notifies the embedder that a postMessage was issued to a remote frame.
    virtual void postMessageEvent(
        WebLocalFrame* sourceFrame,
        WebRemoteFrame* targetFrame,
        WebSecurityOrigin targetOrigin,
        WebDOMMessageEvent) = 0;
};

} // namespace blink

#endif // WebRemoteFrameClient_h
