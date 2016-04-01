// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "web/RemoteFrameOwner.h"

#include "core/frame/LocalFrame.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

RemoteFrameOwner::RemoteFrameOwner(SandboxFlags flags, const WebFrameOwnerProperties& frameOwnerProperties)
    : m_sandboxFlags(flags)
    , m_scrolling(static_cast<ScrollbarMode>(frameOwnerProperties.scrollingMode))
    , m_marginWidth(frameOwnerProperties.marginWidth)
    , m_marginHeight(frameOwnerProperties.marginHeight)
{
}

DEFINE_TRACE(RemoteFrameOwner)
{
    visitor->trace(m_frame);
    FrameOwner::trace(visitor);
}

void RemoteFrameOwner::setScrollingMode(WebFrameOwnerProperties::ScrollingMode mode)
{
    m_scrolling = static_cast<ScrollbarMode>(mode);
}

void RemoteFrameOwner::setContentFrame(Frame& frame)
{
#if !ENABLE(OILPAN)
    // Ownership of RemoteFrameOwner is complicated without Oilpan. Normally, a
    // frame owner like HTMLFrameOwnerElement is kept alive by the DOM tree in
    // its parent frame: the content frame of the frame owner only has a raw
    // pointer back to its owner.
    //
    // However, a parent frame that is remote has no DOM tree, and thus, nothing
    // to keep the remote frame owner alive. Instead, we manually ref() and
    // deref() in setContentFrame() and clearContentFrame() to ensure that the
    // remote frame owner remains live while associated with a frame.
    //
    // In an ideal world, Frame::m_owner would be a RefPtr, which would remove
    // the need for this manual ref() / deref() pair. However, it's non-trivial
    // to make that change, since HTMLFrameOwnerElement (which has FrameOwner
    // has a base class) is already refcounted via a different base class.
    //
    // The other tricky part is WebFrame::swap(): during swap(), there is a
    // period of time when a frame owner won't have an associated content frame.
    // To prevent the refcount from going to zero, WebFrame::swap() must keep a
    // stack reference to the frame owner if it is a remote frame owner.
    ref();
#endif
    m_frame = &frame;
}

void RemoteFrameOwner::clearContentFrame()
{
    DCHECK_EQ(m_frame->owner(), this);
    m_frame = nullptr;
#if !ENABLE(OILPAN)
    // Balance the ref() in setContentFrame().
    deref();
#endif
}

void RemoteFrameOwner::dispatchLoad()
{
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(toLocalFrame(*m_frame));
    webFrame->client()->dispatchLoad();
}

} // namespace blink
