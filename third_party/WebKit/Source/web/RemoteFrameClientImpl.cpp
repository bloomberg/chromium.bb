// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/RemoteFrameClientImpl.h"

#include "platform/exported/WrappedResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"

namespace blink {

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* webFrame)
    : m_webFrame(webFrame)
{
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

} // namespace blink
