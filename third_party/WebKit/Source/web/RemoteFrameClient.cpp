// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/RemoteFrameClient.h"

#include "web/WebRemoteFrameImpl.h"

namespace blink {

RemoteFrameClient::RemoteFrameClient(WebRemoteFrameImpl* webFrame)
    : m_webFrame(webFrame)
{
}

blink::Frame* RemoteFrameClient::opener() const
{
    return toWebCoreFrame(m_webFrame->opener());
}

void RemoteFrameClient::setOpener(blink::Frame*)
{
    // FIXME: Implement.
}

blink::Frame* RemoteFrameClient::parent() const
{
    return toWebCoreFrame(m_webFrame->parent());
}

blink::Frame* RemoteFrameClient::top() const
{
    return toWebCoreFrame(m_webFrame->top());
}

blink::Frame* RemoteFrameClient::previousSibling() const
{
    return toWebCoreFrame(m_webFrame->previousSibling());
}

blink::Frame* RemoteFrameClient::nextSibling() const
{
    return toWebCoreFrame(m_webFrame->nextSibling());
}

blink::Frame* RemoteFrameClient::firstChild() const
{
    return toWebCoreFrame(m_webFrame->firstChild());
}

blink::Frame* RemoteFrameClient::lastChild() const
{
    return toWebCoreFrame(m_webFrame->lastChild());
}

} // namespace blink
