// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RemoteFrame.h"

#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WindowProxyManager.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/frame/RemoteDOMWindow.h"
#include "core/frame/RemoteFrameClient.h"
#include "core/frame/RemoteFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutPart.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebLayer.h"

namespace blink {

inline RemoteFrame::RemoteFrame(RemoteFrameClient* client, FrameHost* host, FrameOwner* owner)
    : Frame(client, host, owner)
    , m_securityContext(RemoteSecurityContext::create())
    , m_domWindow(RemoteDOMWindow::create(*this))
    , m_windowProxyManager(WindowProxyManager::create(*this))
    , m_remotePlatformLayer(nullptr)
{
}

PassRefPtrWillBeRawPtr<RemoteFrame> RemoteFrame::create(RemoteFrameClient* client, FrameHost* host, FrameOwner* owner)
{
    return adoptRefWillBeNoop(new RemoteFrame(client, host, owner));
}

RemoteFrame::~RemoteFrame()
{
}

DEFINE_TRACE(RemoteFrame)
{
    visitor->trace(m_view);
    visitor->trace(m_domWindow);
    visitor->trace(m_windowProxyManager);
    Frame::trace(visitor);
}

DOMWindow* RemoteFrame::domWindow() const
{
    return m_domWindow.get();
}

WindowProxy* RemoteFrame::windowProxy(DOMWrapperWorld& world)
{
    WindowProxy* windowProxy = m_windowProxyManager->windowProxy(world);
    ASSERT(windowProxy);
    windowProxy->initializeIfNeeded();
    return windowProxy;
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
    // Frame::detach() requires the caller to keep a reference to this, since
    // otherwise it may clear the last reference to this, causing it to be
    // deleted, which can cause a use-after-free.
    RefPtrWillBeRawPtr<RemoteFrame> protect(this);
    detachChildren();
    if (!client())
        return;
    m_windowProxyManager->clearForClose();
    setView(nullptr);
    Frame::detach();
}

RemoteSecurityContext* RemoteFrame::securityContext() const
{
    return m_securityContext.get();
}

void RemoteFrame::disconnectOwnerElement()
{
    // The RemotePlatformLayer needs to be cleared in disconnectOwnerElement()
    // because it must happen on WebFrame::swap() and Frame::detach().
    if (m_remotePlatformLayer)
        setRemotePlatformLayer(nullptr);

    Frame::disconnectOwnerElement();
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
    setView(nullptr);
    if (!tree().parent() || !tree().parent()->isLocalFrame()) {
        // FIXME: This is not the right place to clear the previous frame's
        // widget. We do it here because the LocalFrame cleanup after a swap is
        // still work in progress.
        if (ownerLayoutObject()) {
            HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
            ASSERT(owner);
            owner->setWidget(nullptr);
        }

        return;
    }

    RefPtrWillBeRawPtr<RemoteFrameView> view = RemoteFrameView::create(this);
    setView(view);

    if (ownerLayoutObject()) {
        HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
        ASSERT(owner);
        owner->setWidget(view);
    }
}

RemoteFrameClient* RemoteFrame::remoteFrameClient() const
{
    return static_cast<RemoteFrameClient*>(client());
}

void RemoteFrame::setRemotePlatformLayer(WebLayer* layer)
{
    if (m_remotePlatformLayer)
        GraphicsLayer::unregisterContentsLayer(m_remotePlatformLayer);
    m_remotePlatformLayer = layer;
    if (m_remotePlatformLayer)
        GraphicsLayer::registerContentsLayer(layer);

    ASSERT(owner());
    toHTMLFrameOwnerElement(owner())->setNeedsCompositingUpdate();
    if (LayoutPart* layoutObject = ownerLayoutObject())
        layoutObject->layer()->updateSelfPaintingLayer();
}

} // namespace blink
