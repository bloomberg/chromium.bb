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
    virtual void detached() override;

    virtual Frame* opener() const override;
    virtual void setOpener(Frame*) override;

    virtual Frame* parent() const override;
    virtual Frame* top() const override;
    virtual Frame* previousSibling() const override;
    virtual Frame* nextSibling() const override;
    virtual Frame* firstChild() const override;
    virtual Frame* lastChild() const override;

    virtual bool willCheckAndDispatchMessageEvent(SecurityOrigin*, MessageEvent*, LocalFrame*) const override;

    // RemoteFrameClient overrides:
    virtual void navigate(const ResourceRequest&, bool shouldReplaceCurrentEntry) override;
    virtual void forwardInputEvent(Event*) override;

    WebRemoteFrameImpl* webFrame() const { return m_webFrame; }

private:
    WebRemoteFrameImpl* m_webFrame;
};

} // namespace blink

#endif // RemoteFrameClientImpl_h
