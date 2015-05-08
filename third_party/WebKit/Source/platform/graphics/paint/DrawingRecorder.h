// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingRecorder_h
#define DrawingRecorder_h

#include "platform/PlatformExport.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"

#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT DrawingRecorder {
public:
    DrawingRecorder(GraphicsContext&, const DisplayItemClientWrapper&, DisplayItem::Type, const FloatRect& cullRect);
    ~DrawingRecorder();

    bool canUseCachedDrawing() const
    {
#if ENABLE(ASSERT)
        m_checkedCachedDrawing = true;
#endif
        return m_canUseCachedDrawing;
    }

#if ENABLE(ASSERT)
    void setSkipUnderInvalidationChecking() { m_skipUnderInvalidationChecking = true; }
#endif

private:
    GraphicsContext& m_context;
    DisplayItemClientWrapper m_displayItemClient;
    const DisplayItem::Type m_displayItemType;
    bool m_canUseCachedDrawing;
#if ENABLE(ASSERT)
    mutable bool m_checkedCachedDrawing;
    size_t m_displayItemPosition;
    bool m_skipUnderInvalidationChecking;
#endif
};

} // namespace blink

#endif // DrawingRecorder_h
