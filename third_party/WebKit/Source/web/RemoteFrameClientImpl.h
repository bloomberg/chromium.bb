// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClientImpl_h
#define RemoteFrameClientImpl_h

#include "core/frame/RemoteFrameClient.h"

namespace blink {
class WebRemoteFrameImpl;

class RemoteFrameClientImpl : public RemoteFrameClient {
public:
    explicit RemoteFrameClientImpl(WebRemoteFrameImpl*);

    // FrameClient overrides:
    virtual Frame* opener() const OVERRIDE;
    virtual void setOpener(Frame*) OVERRIDE;

    virtual Frame* parent() const OVERRIDE;
    virtual Frame* top() const OVERRIDE;
    virtual Frame* previousSibling() const OVERRIDE;
    virtual Frame* nextSibling() const OVERRIDE;
    virtual Frame* firstChild() const OVERRIDE;
    virtual Frame* lastChild() const OVERRIDE;

    virtual bool willCheckAndDispatchMessageEvent(SecurityOrigin*, MessageEvent*, LocalFrame*) const OVERRIDE;

    // RemoteFrameClient overrides:
    virtual void navigate(const ResourceRequest&, bool shouldReplaceCurrentEntry) OVERRIDE;

    WebRemoteFrameImpl* webFrame() const { return m_webFrame; }

private:
    WebRemoteFrameImpl* m_webFrame;
};

} // namespace blink

#endif // RemoteFrameClientImpl_h
