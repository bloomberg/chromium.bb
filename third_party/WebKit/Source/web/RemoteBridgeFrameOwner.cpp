// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "web/RemoteBridgeFrameOwner.h"

#include "public/web/WebFrameClient.h"

namespace blink {

RemoteBridgeFrameOwner::RemoteBridgeFrameOwner(PassRefPtrWillBeRawPtr<WebLocalFrameImpl> frame, SandboxFlags flags, const WebFrameOwnerProperties& frameOwnerProperties)
    : m_frame(frame)
    , m_sandboxFlags(flags)
    , m_scrolling(static_cast<ScrollbarMode>(frameOwnerProperties.scrollingMode))
    , m_marginWidth(frameOwnerProperties.marginWidth)
    , m_marginHeight(frameOwnerProperties.marginHeight)
{
}

DEFINE_TRACE(RemoteBridgeFrameOwner)
{
    visitor->trace(m_frame);
    FrameOwner::trace(visitor);
}

void RemoteBridgeFrameOwner::setScrollingMode(WebFrameOwnerProperties::ScrollingMode mode)
{
    m_scrolling = static_cast<ScrollbarMode>(mode);
}

void RemoteBridgeFrameOwner::dispatchLoad()
{
    if (m_frame->client())
        m_frame->client()->dispatchLoad();
}

} // namespace blink
