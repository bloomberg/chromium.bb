// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/RemoteFrameClientImpl.h"

#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/WheelEvent.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/rendering/RenderPart.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"

namespace blink {

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* webFrame)
    : m_webFrame(webFrame)
{
}

void RemoteFrameClientImpl::detached()
{
    // FIXME: Implement.
}

Frame* RemoteFrameClientImpl::opener() const
{
    return toCoreFrame(m_webFrame->opener());
}

void RemoteFrameClientImpl::setOpener(Frame*)
{
    // FIXME: Implement.
}

Frame* RemoteFrameClientImpl::parent() const
{
    return toCoreFrame(m_webFrame->parent());
}

Frame* RemoteFrameClientImpl::top() const
{
    return toCoreFrame(m_webFrame->top());
}

Frame* RemoteFrameClientImpl::previousSibling() const
{
    return toCoreFrame(m_webFrame->previousSibling());
}

Frame* RemoteFrameClientImpl::nextSibling() const
{
    return toCoreFrame(m_webFrame->nextSibling());
}

Frame* RemoteFrameClientImpl::firstChild() const
{
    return toCoreFrame(m_webFrame->firstChild());
}

Frame* RemoteFrameClientImpl::lastChild() const
{
    return toCoreFrame(m_webFrame->lastChild());
}

bool RemoteFrameClientImpl::willCheckAndDispatchMessageEvent(
    SecurityOrigin* target, MessageEvent* event, LocalFrame* sourceFrame) const
{
    if (m_webFrame->client())
        m_webFrame->client()->postMessageEvent(WebLocalFrameImpl::fromFrame(sourceFrame), m_webFrame, WebSecurityOrigin(target), WebDOMMessageEvent(event));
    return true;
}

void RemoteFrameClientImpl::navigate(const ResourceRequest& request, bool shouldReplaceCurrentEntry)
{
    if (m_webFrame->client())
        m_webFrame->client()->navigate(WrappedResourceRequest(request), shouldReplaceCurrentEntry);
}

// FIXME: Remove this code once we have input routing in the browser
// process. See http://crbug.com/339659.
void RemoteFrameClientImpl::forwardInputEvent(Event* event)
{
    // This is only called when we have out-of-process iframes, which
    // need to forward input events across processes.
    // FIXME: Add a check for out-of-process iframes enabled.
    OwnPtr<WebInputEvent> webEvent;
    if (event->isKeyboardEvent())
        webEvent = adoptPtr(new WebKeyboardEventBuilder(*static_cast<KeyboardEvent*>(event)));
    else if (event->isMouseEvent())
        webEvent = adoptPtr(new WebMouseEventBuilder(m_webFrame->frame()->view(), toCoreFrame(m_webFrame)->ownerRenderer(), *static_cast<MouseEvent*>(event)));
    else if (event->isWheelEvent())
        webEvent = adoptPtr(new WebMouseWheelEventBuilder(m_webFrame->frame()->view(), toCoreFrame(m_webFrame)->ownerRenderer(), *static_cast<WheelEvent*>(event)));

    // Other or internal Blink events should not be forwarded.
    if (!webEvent || webEvent->type == WebInputEvent::Undefined)
        return;

    m_webFrame->client()->forwardInputEvent(webEvent.get());
}

} // namespace blink
