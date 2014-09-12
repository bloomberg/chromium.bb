// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClient_h
#define RemoteFrameClient_h

#include "core/frame/FrameClient.h"

namespace blink {

class WebRemoteFrameImpl;

class RemoteFrameClient : public FrameClient {
public:
    explicit RemoteFrameClient(WebRemoteFrameImpl*);

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

    WebRemoteFrameImpl* webFrame() const { return m_webFrame; }

private:
    WebRemoteFrameImpl* m_webFrame;
};

} // namespace blink

#endif // RemoteFrameClient_h
