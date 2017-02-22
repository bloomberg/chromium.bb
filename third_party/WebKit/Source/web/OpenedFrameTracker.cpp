// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/OpenedFrameTracker.h"

#include "platform/heap/Handle.h"
#include "public/web/WebFrame.h"

namespace blink {

OpenedFrameTracker::OpenedFrameTracker() {}

OpenedFrameTracker::~OpenedFrameTracker() {
  DCHECK(isEmpty());
}

bool OpenedFrameTracker::isEmpty() const {
  return m_openedFrames.isEmpty();
}

void OpenedFrameTracker::add(WebFrame* frame) {
  m_openedFrames.insert(frame);
}

void OpenedFrameTracker::remove(WebFrame* frame) {
  m_openedFrames.erase(frame);
}

void OpenedFrameTracker::transferTo(WebFrame* opener) {
  // Copy the set of opened frames, since changing the owner will mutate this
  // set.
  HashSet<WebFrame*> frames(m_openedFrames);
  for (WebFrame* frame : frames)
    frame->setOpener(opener);
}

}  // namespace blink
