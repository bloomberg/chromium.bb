// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipScope_h
#define ClipScope_h

#include "platform/graphics/GraphicsContext.h"
#include "wtf/Allocator.h"

namespace blink {

class ClipScope {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(ClipScope);
public:
    ClipScope(GraphicsContext& context)
        : m_context(context)
        , m_clipCount(0) { }
    ~ClipScope();

    void clip(const LayoutRect& clipRect, SkRegion::Op operation);

private:
    GraphicsContext& m_context;
    int m_clipCount;
};

} // namespace blink

#endif // ClipScope_h
