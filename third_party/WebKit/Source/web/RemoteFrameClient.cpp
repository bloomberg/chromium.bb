// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/RemoteFrameClient.h"

#include "platform/weborigin/SecurityOrigin.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"

namespace blink {

RemoteFrameClient::RemoteFrameClient(WebRemoteFrameImpl* webFrame)
    : m_webFrame(webFrame)
{
}

Frame* RemoteFrameClient::opener() const
{
    return toCoreFrame(m_webFrame->opener());
}

void RemoteFrameClient::setOpener(Frame*)
{
    // FIXME: Implement.
}

Frame* RemoteFrameClient::parent() const
{
    return toCoreFrame(m_webFrame->parent());
}

Frame* RemoteFrameClient::top() const
{
    return toCoreFrame(m_webFrame->top());
}

Frame* RemoteFrameClient::previousSibling() const
{
    return toCoreFrame(m_webFrame->previousSibling());
}

Frame* RemoteFrameClient::nextSibling() const
{
    return toCoreFrame(m_webFrame->nextSibling());
}

Frame* RemoteFrameClient::firstChild() const
{
    return toCoreFrame(m_webFrame->firstChild());
}

Frame* RemoteFrameClient::lastChild() const
{
    return toCoreFrame(m_webFrame->lastChild());
}

bool RemoteFrameClient::willCheckAndDispatchMessageEvent(
    SecurityOrigin* target, MessageEvent* event, LocalFrame* sourceFrame) const
{
    if (m_webFrame->client())
        m_webFrame->client()->postMessageEvent(WebLocalFrameImpl::fromFrame(sourceFrame), m_webFrame, WebSecurityOrigin(target), WebDOMMessageEvent(event));
    return true;
}

} // namespace blink
