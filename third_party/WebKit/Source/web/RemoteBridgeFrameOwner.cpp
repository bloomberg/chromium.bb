// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "web/RemoteBridgeFrameOwner.h"

#include "core/frame/LocalFrame.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

RemoteBridgeFrameOwner::RemoteBridgeFrameOwner(SandboxFlags flags, const WebFrameOwnerProperties& frameOwnerProperties)
    : m_sandboxFlags(flags)
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

void RemoteBridgeFrameOwner::setContentFrame(Frame& frame)
{
    m_frame = &frame;
}

void RemoteBridgeFrameOwner::clearContentFrame()
{
    ASSERT(m_frame->owner() == this);
    m_frame = nullptr;
}

void RemoteBridgeFrameOwner::dispatchLoad()
{
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(toLocalFrame(*m_frame));
    webFrame->client()->dispatchLoad();
}

} // namespace blink
