// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/web/WebFrame.h"

#include "web/OpenedFrameTracker.h"
#include <algorithm>


namespace blink {

void WebFrame::swap(WebFrame* frame)
{
    using std::swap;

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

} // namespace blink
