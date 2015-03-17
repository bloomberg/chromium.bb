// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrame_h
#define WebRemoteFrame_h

#include "public/web/WebFrame.h"
#include "public/web/WebSandboxFlags.h"

namespace blink {

class WebFrameClient;
class WebRemoteFrameClient;

class WebRemoteFrame : public WebFrame {
public:
    BLINK_EXPORT static WebRemoteFrame* create(WebRemoteFrameClient*);

    virtual WebLocalFrame* createLocalChild(const WebString& name, WebSandboxFlags, WebFrameClient*) = 0;

    virtual WebRemoteFrame* createRemoteChild(const WebString& name, WebSandboxFlags, WebRemoteFrameClient*) = 0;

    // Transfer initial drawing parameters from a local frame.
    virtual void initializeFromFrame(WebLocalFrame*) const = 0;

    // Set security origin replicated from another process.
    virtual void setReplicatedOrigin(const WebSecurityOrigin&) const = 0;

    // Set sandbox flags replicated from another process.
    virtual void setReplicatedSandboxFlags(WebSandboxFlags) const = 0;

    // Set frame name replicated from another process.
    virtual void setReplicatedName(const WebString&) const = 0;

    virtual void DispatchLoadEventForFrameOwner() const = 0;

    virtual void didStartLoading() = 0;
    virtual void didStopLoading() = 0;
};

} // namespace blink

#endif // WebRemoteFrame_h
