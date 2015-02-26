// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrame_h
#define RemoteFrame_h

#include "core/dom/RemoteSecurityContext.h"
#include "core/frame/Frame.h"

namespace blink {

class Event;
class RemoteDOMWindow;
class RemoteFrameClient;
class RemoteFrameView;
class WebLayer;
class WindowProxyManager;

class RemoteFrame: public Frame {
public:
    static PassRefPtrWillBeRawPtr<RemoteFrame> create(RemoteFrameClient*, FrameHost*, FrameOwner*);

    virtual ~RemoteFrame();

    // Frame overrides:
    DECLARE_VIRTUAL_TRACE();
    bool isRemoteFrame() const override { return true; }
    DOMWindow* domWindow() const override;
    WindowProxy* windowProxy(DOMWrapperWorld&) override;
    void navigate(Document& originDocument, const KURL&, bool lockBackForwardList) override;
    void reload(ReloadPolicy, ClientRedirectPolicy) override;
    void detach() override;
    RemoteSecurityContext* securityContext() const override;
    void printNavigationErrorMessage(const Frame&, const char* reason) override { }
    void disconnectOwnerElement() override;

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    void forwardInputEvent(Event*);

    void setRemotePlatformLayer(WebLayer*);
    WebLayer* remotePlatformLayer() const { return m_remotePlatformLayer; }

    void setView(PassRefPtrWillBeRawPtr<RemoteFrameView>);
    void createView();

    RemoteFrameView* view() const;

private:
    RemoteFrame(RemoteFrameClient*, FrameHost*, FrameOwner*);

    // Internal Frame helper overrides:
    WindowProxyManager* windowProxyManager() const override { return m_windowProxyManager.get(); }

    RemoteFrameClient* remoteFrameClient() const;

    RefPtrWillBeMember<RemoteFrameView> m_view;
    RefPtr<RemoteSecurityContext> m_securityContext;
    RefPtrWillBeMember<RemoteDOMWindow> m_domWindow;
    OwnPtrWillBeMember<WindowProxyManager> m_windowProxyManager;
    WebLayer* m_remotePlatformLayer;
};

inline RemoteFrameView* RemoteFrame::view() const
{
    return m_view.get();
}

DEFINE_TYPE_CASTS(RemoteFrame, Frame, remoteFrame, remoteFrame->isRemoteFrame(), remoteFrame.isRemoteFrame());

} // namespace blink

#endif // RemoteFrame_h
