// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/graphics/paint/ClipRecorderStack.h"

namespace blink {

ClipRecorderStack::~ClipRecorderStack()
{
    Vector<OwnPtr<ClipRecorder>>::iterator end = m_clipRecorders.end();
    for (Vector<OwnPtr<ClipRecorder>>::iterator iter = m_clipRecorders.begin(); iter != end; ++iter)
        iter->clear();
    ASSERT(m_context->clipRecorderStack() == this);
    m_context->setClipRecorderStack(m_parent);
}

} // namespace blink
