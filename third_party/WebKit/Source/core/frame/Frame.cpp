/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/frame/Frame.h"

#include "core/dom/DocumentType.h"
#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameHost.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderPart.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/WebLayer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCountedLeakCounter.h"

namespace blink {

using namespace HTMLNames;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, frameCounter, ("Frame"));

Frame::Frame(FrameClient* client, FrameHost* host, FrameOwner* owner)
    : m_treeNode(this)
    , m_host(host)
    , m_owner(owner)
    , m_client(client)
    , m_remotePlatformLayer(0)
{
    ASSERT(page());

#ifndef NDEBUG
    frameCounter.increment();
#endif

    if (m_owner) {
        if (m_owner->isLocal())
            toHTMLFrameOwnerElement(m_owner)->setContentFrame(*this);
    } else {
        page()->setMainFrame(this);
    }
}

Frame::~Frame()
{
#if ENABLE(OILPAN)
    ASSERT(!m_owner);
#else
    // FIXME: We should not be doing all this work inside the destructor
    disconnectOwnerElement();
    setDOMWindow(nullptr);
#endif

#ifndef NDEBUG
    frameCounter.decrement();
#endif
}

void Frame::trace(Visitor* visitor)
{
    visitor->trace(m_treeNode);
    visitor->trace(m_host);
    visitor->trace(m_owner);
    visitor->trace(m_domWindow);
}

void Frame::detach()
{
    // client() should never be null because that means we somehow re-entered
    // the frame detach code... but it is sometimes.
    // FIXME: Understand why this is happening so we can document this insanity.
    // http://crbug.com/371084 is a probable explanation.
    if (!client())
        return;
    // After this, we must no longer talk to the client since this clears
    // its owning reference back to our owning LocalFrame.
    m_client->detached();
    m_client = nullptr;
    m_host = nullptr;
}

void Frame::detachChildren()
{
    typedef WillBeHeapVector<RefPtrWillBeMember<Frame> > FrameVector;
    FrameVector childrenToDetach;
    childrenToDetach.reserveCapacity(tree().childCount());
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling())
        childrenToDetach.append(child);
    FrameVector::iterator end = childrenToDetach.end();
    for (FrameVector::iterator it = childrenToDetach.begin(); it != end; ++it)
        (*it)->detach();
}

FrameHost* Frame::host() const
{
    return m_host;
}

Page* Frame::page() const
{
    if (m_host)
        return &m_host->page();
    return 0;
}

Settings* Frame::settings() const
{
    if (m_host)
        return &m_host->settings();
    return 0;
}

void Frame::setDOMWindow(PassRefPtrWillBeRawPtr<LocalDOMWindow> domWindow)
{
    if (m_domWindow)
        m_domWindow->reset();

    m_domWindow = domWindow;
}

static ChromeClient& emptyChromeClient()
{
    DEFINE_STATIC_LOCAL(EmptyChromeClient, client, ());
    return client;
}

ChromeClient& Frame::chromeClient() const
{
    if (Page* page = this->page())
        return page->chrome().client();
    return emptyChromeClient();
}

RenderPart* Frame::ownerRenderer() const
{
    if (!deprecatedLocalOwner())
        return 0;
    RenderObject* object = deprecatedLocalOwner()->renderer();
    if (!object)
        return 0;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since ownerElement() would be 0 when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    if (!object->isRenderPart())
        return 0;
    return toRenderPart(object);
}

void Frame::setRemotePlatformLayer(WebLayer* layer)
{
    if (m_remotePlatformLayer)
        GraphicsLayer::unregisterContentsLayer(m_remotePlatformLayer);
    m_remotePlatformLayer = layer;
    if (m_remotePlatformLayer)
        GraphicsLayer::registerContentsLayer(layer);

    ASSERT(owner());
    toHTMLFrameOwnerElement(owner())->setNeedsCompositingUpdate();
    if (RenderPart* renderer = ownerRenderer())
        renderer->layer()->updateSelfPaintingLayer();
}

bool Frame::isMainFrame() const
{
    Page* page = this->page();
    return page && this == page->mainFrame();
}

bool Frame::isLocalRoot() const
{
    if (isRemoteFrame())
        return false;

    if (!tree().parent())
        return true;

    return tree().parent()->isRemoteFrame();
}

void Frame::disconnectOwnerElement()
{
    if (m_owner) {
        if (m_owner->isLocal())
            toHTMLFrameOwnerElement(m_owner)->clearContentFrame();
    }
    m_owner = nullptr;
}

HTMLFrameOwnerElement* Frame::deprecatedLocalOwner() const
{
    return m_owner && m_owner->isLocal() ? toHTMLFrameOwnerElement(m_owner) : 0;
}

} // namespace blink
