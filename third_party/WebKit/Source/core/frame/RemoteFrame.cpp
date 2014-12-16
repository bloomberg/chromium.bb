// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RemoteFrame.h"

#include "core/dom/RemoteSecurityContext.h"
#include "core/frame/RemoteDOMWindow.h"
#include "core/frame/RemoteFrameClient.h"
#include "core/frame/RemoteFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

inline RemoteFrame::RemoteFrame(RemoteFrameClient* client, FrameHost* host, FrameOwner* owner)
    : Frame(client, host, owner)
    , m_securityContext(RemoteSecurityContext::create())
    , m_domWindow(RemoteDOMWindow::create(*this))
{
}

PassRefPtrWillBeRawPtr<RemoteFrame> RemoteFrame::create(RemoteFrameClient* client, FrameHost* host, FrameOwner* owner)
{
    return adoptRefWillBeNoop(new RemoteFrame(client, host, owner));
}

RemoteFrame::~RemoteFrame()
{
    setView(nullptr);
}

void RemoteFrame::trace(Visitor* visitor)
{
    visitor->trace(m_view);
    visitor->trace(m_domWindow);
    Frame::trace(visitor);
}

DOMWindow* RemoteFrame::domWindow() const
{
    return m_domWindow.get();
}

void RemoteFrame::navigate(Document& originDocument, const KURL& url, bool lockBackForwardList)
{
    // The process where this frame actually lives won't have sufficient information to determine
    // correct referrer, since it won't have access to the originDocument. Set it now.
    ResourceRequest request(url);
    request.setHTTPReferrer(SecurityPolicy::generateReferrer(originDocument.referrerPolicy(), url, originDocument.outgoingReferrer()));
    remoteFrameClient()->navigate(request, lockBackForwardList);
}

void RemoteFrame::reload(ReloadPolicy reloadPolicy, ClientRedirectPolicy clientRedirectPolicy)
{
    remoteFrameClient()->reload(reloadPolicy, clientRedirectPolicy);
}

void RemoteFrame::detach()
{
    detachChildren();
    if (!client())
        return;
    Frame::detach();
}

RemoteSecurityContext* RemoteFrame::securityContext() const
{
    return m_securityContext.get();
}

void RemoteFrame::forwardInputEvent(Event* event)
{
    remoteFrameClient()->forwardInputEvent(event);
}

void RemoteFrame::setView(PassRefPtrWillBeRawPtr<RemoteFrameView> view)
{
    // Oilpan: as RemoteFrameView performs no finalization actions,
    // no explicit dispose() of it needed here. (cf. FrameView::dispose().)
    m_view = view;
}

void RemoteFrame::createView()
{
    RefPtrWillBeRawPtr<RemoteFrameView> view = RemoteFrameView::create(this);
    setView(view);

    if (ownerRenderer()) {
        HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
        ASSERT(owner);
        owner->setWidget(view);
    }
}

RemoteFrameClient* RemoteFrame::remoteFrameClient() const
{
    return static_cast<RemoteFrameClient*>(client());
}

} // namespace blink
