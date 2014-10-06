// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/web/WebFrame.h"

#include "core/frame/RemoteFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "platform/UserGestureIndicator.h"
#include "platform/heap/Handle.h"
#include "web/OpenedFrameTracker.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include <algorithm>

namespace blink {

Frame* toCoreFrame(const WebFrame* frame)
{
    if (!frame)
        return 0;

    return frame->isWebLocalFrame()
        ? static_cast<Frame*>(toWebLocalFrameImpl(frame)->frame())
        : toWebRemoteFrameImpl(frame)->frame();
}

bool WebFrame::swap(WebFrame* frame)
{
    using std::swap;
    RefPtrWillBeRawPtr<Frame> oldFrame = toCoreFrame(this);

    // All child frames must be detached first.
    oldFrame->detachChildren();

    // If the frame has been detached during detaching its children, return
    // immediately.
    // FIXME: There is no unit test for this condition, so one needs to be
    // written.
    if (!oldFrame->host())
        return false;

    // The frame being swapped in should not have a Frame associated
    // with it yet.
    ASSERT(!toCoreFrame(frame));

    if (m_parent) {
        if (m_parent->m_firstChild == this)
            m_parent->m_firstChild = frame;
        if (m_parent->m_lastChild == this)
            m_parent->m_lastChild = frame;
        swap(m_parent, frame->m_parent);
    }

    if (m_previousSibling) {
        m_previousSibling->m_nextSibling = frame;
        swap(m_previousSibling, frame->m_previousSibling);
    }
    if (m_nextSibling) {
        m_nextSibling->m_previousSibling = frame;
        swap(m_nextSibling, frame->m_nextSibling);
    }

    if (m_opener) {
        m_opener->m_openedFrameTracker->remove(this);
        m_opener->m_openedFrameTracker->add(frame);
        swap(m_opener, frame->m_opener);
    }
    if (!m_openedFrameTracker->isEmpty()) {
        m_openedFrameTracker->updateOpener(frame);
        frame->m_openedFrameTracker.reset(m_openedFrameTracker.release());
    }

    // Finally, clone the state of the current Frame into one matching
    // the type of the passed in WebFrame.
    // FIXME: This is a bit clunky; this results in pointless decrements and
    // increments of connected subframes.
    FrameOwner* owner = oldFrame->owner();
    oldFrame->disconnectOwnerElement();
    if (frame->isWebLocalFrame()) {
        toWebLocalFrameImpl(frame)->initializeCoreFrame(oldFrame->host(), owner, oldFrame->tree().name(), nullAtom);
    } else {
        toWebRemoteFrameImpl(frame)->initializeCoreFrame(oldFrame->host(), owner, oldFrame->tree().name());
    }

    return true;
}

void WebFrame::detach()
{
    toCoreFrame(this)->detach();
}

WebFrame* WebFrame::opener() const
{
    return m_opener;
}

void WebFrame::setOpener(WebFrame* opener)
{
    if (m_opener)
        m_opener->m_openedFrameTracker->remove(this);
    if (opener)
        opener->m_openedFrameTracker->add(this);
    m_opener = opener;
}

void WebFrame::appendChild(WebFrame* child)
{
    // FIXME: Original code asserts that the frames have the same Page. We
    // should add an equivalent check... figure out what.
    child->m_parent = this;
    WebFrame* oldLast = m_lastChild;
    m_lastChild = child;

    if (oldLast) {
        child->m_previousSibling = oldLast;
        oldLast->m_nextSibling = child;
    } else {
        m_firstChild = child;
    }

    toCoreFrame(this)->tree().invalidateScopedChildCount();
}

void WebFrame::removeChild(WebFrame* child)
{
    child->m_parent = 0;

    if (m_firstChild == child)
        m_firstChild = child->m_nextSibling;
    else
        child->m_previousSibling->m_nextSibling = child->m_nextSibling;

    if (m_lastChild == child)
        m_lastChild = child->m_previousSibling;
    else
        child->m_nextSibling->m_previousSibling = child->m_previousSibling;

    child->m_previousSibling = child->m_nextSibling = 0;

    toCoreFrame(this)->tree().invalidateScopedChildCount();
}

WebFrame* WebFrame::parent() const
{
    return m_parent;
}

WebFrame* WebFrame::top() const
{
    WebFrame* frame = const_cast<WebFrame*>(this);
    for (WebFrame* parent = frame; parent; parent = parent->m_parent)
        frame = parent;
    return frame;
}

WebFrame* WebFrame::firstChild() const
{
    return m_firstChild;
}

WebFrame* WebFrame::lastChild() const
{
    return m_lastChild;
}

WebFrame* WebFrame::previousSibling() const
{
    return m_previousSibling;
}

WebFrame* WebFrame::nextSibling() const
{
    return m_nextSibling;
}

WebFrame* WebFrame::traversePrevious(bool wrap) const
{
    if (Frame* frame = toCoreFrame(this))
        return fromFrame(frame->tree().traversePreviousWithWrap(wrap));
    return 0;
}

WebFrame* WebFrame::traverseNext(bool wrap) const
{
    if (Frame* frame = toCoreFrame(this))
        return fromFrame(frame->tree().traverseNextWithWrap(wrap));
    return 0;
}

WebFrame* WebFrame::findChildByName(const WebString& name) const
{
    Frame* frame = toCoreFrame(this);
    if (!frame)
        return 0;
    // FIXME: It's not clear this should ever be called to find a remote frame.
    // Perhaps just disallow that completely?
    return fromFrame(frame->tree().child(name));
}

WebFrame* WebFrame::fromFrame(Frame* frame)
{
    if (!frame)
        return 0;

    if (frame->isLocalFrame())
        return WebLocalFrameImpl::fromFrame(toLocalFrame(*frame));
    return WebRemoteFrameImpl::fromFrame(toRemoteFrame(*frame));
}

WebFrame::WebFrame()
    : m_parent(0)
    , m_previousSibling(0)
    , m_nextSibling(0)
    , m_firstChild(0)
    , m_lastChild(0)
    , m_opener(0)
    , m_openedFrameTracker(new OpenedFrameTracker)
{
}

WebFrame::~WebFrame()
{
    m_openedFrameTracker.reset(0);
}

#if ENABLE(OILPAN)
void WebFrame::traceFrame(Visitor* visitor, WebFrame* frame)
{
    if (!frame)
        return;

    if (frame->isWebLocalFrame())
        visitor->trace(toWebLocalFrameImpl(frame));
    else
        visitor->trace(toWebRemoteFrameImpl(frame));
}

void WebFrame::traceFrames(Visitor* visitor, WebFrame* frame)
{
    ASSERT(frame);
    traceFrame(visitor, frame->m_parent);
    for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
        traceFrame(visitor, child);
    // m_opener is a weak reference.
    frame->m_openedFrameTracker->traceFrames(visitor);
}

bool WebFrame::isFrameAlive(Visitor* visitor, const WebFrame* frame)
{
    if (!frame)
        return true;

    if (frame->isWebLocalFrame())
        return visitor->isAlive(toWebLocalFrameImpl(frame));

    return visitor->isAlive(toWebRemoteFrameImpl(frame));
}

void WebFrame::clearWeakFrames(Visitor* visitor)
{
    if (!isFrameAlive(visitor, m_opener))
        m_opener = nullptr;
}
#endif

} // namespace blink
