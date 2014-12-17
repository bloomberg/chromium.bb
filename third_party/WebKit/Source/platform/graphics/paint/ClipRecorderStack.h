// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipRecorderStack_h
#define ClipRecorderStack_h

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/ClipRecorder.h"

namespace blink {

class PLATFORM_EXPORT ClipRecorderStack {
public:
    ClipRecorderStack(GraphicsContext* context)
        : m_parent(context->clipRecorderStack())
        , m_context(context)
    {
        context->setClipRecorderStack(this);
    }

    ~ClipRecorderStack();

    void addClipRecorder(PassOwnPtr<ClipRecorder> clipRecorder)
    {
        ASSERT(m_context->clipRecorderStack() == this);
        m_clipRecorders.append(clipRecorder);
    }
private:
    ClipRecorderStack* m_parent;
    Vector<OwnPtr<ClipRecorder>> m_clipRecorders;
    GraphicsContext* m_context;
    WTF_MAKE_NONCOPYABLE(ClipRecorderStack);
};

} // namespace blink

#endif // ClipRecorderStack_h
