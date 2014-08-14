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

Frame* RemoteFrameClient::opener() const
{
    return toWebCoreFrame(m_webFrame->opener());
}

void RemoteFrameClient::setOpener(Frame*)
{
    // FIXME: Implement.
}

Frame* RemoteFrameClient::parent() const
{
    return toWebCoreFrame(m_webFrame->parent());
}

Frame* RemoteFrameClient::top() const
{
    return toWebCoreFrame(m_webFrame->top());
}

Frame* RemoteFrameClient::previousSibling() const
{
    return toWebCoreFrame(m_webFrame->previousSibling());
}

Frame* RemoteFrameClient::nextSibling() const
{
    return toWebCoreFrame(m_webFrame->nextSibling());
}

Frame* RemoteFrameClient::firstChild() const
{
    return toWebCoreFrame(m_webFrame->firstChild());
}

Frame* RemoteFrameClient::lastChild() const
{
    return toWebCoreFrame(m_webFrame->lastChild());
}

} // namespace blink
