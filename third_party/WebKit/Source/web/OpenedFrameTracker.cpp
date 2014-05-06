// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/OpenedFrameTracker.h"

#include "public/web/WebFrame.h"

namespace blink {

OpenedFrameTracker::OpenedFrameTracker()
{
}

OpenedFrameTracker::~OpenedFrameTracker()
{
    HashSet<WebFrame*>::iterator end = m_openedFrames.end();
    for (HashSet<WebFrame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->m_opener = 0;
}

void OpenedFrameTracker::add(WebFrame* frame)
{
    m_openedFrames.add(frame);
}

void OpenedFrameTracker::remove(WebFrame* frame)
{
    m_openedFrames.remove(frame);
}

} // namespace blink
