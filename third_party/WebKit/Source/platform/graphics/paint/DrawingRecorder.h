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
    explicit DrawingRecorder(GraphicsContext*, DisplayItemClient, DisplayItem::Type, const FloatRect& bounds);

    ~DrawingRecorder();

    bool canUseCachedDrawing() const
    {
#if ENABLE(ASSERT)
        m_checkedCachedDrawing = true;
#endif
        return m_canUseCachedDrawing;
    }

#ifndef NDEBUG
    void setClientDebugString(const WTF::String&);
#endif

private:
    GraphicsContext* m_context;
    DisplayItemClient m_displayItemClient;
    const DisplayItem::Type m_displayItemType;
    bool m_canUseCachedDrawing;
#if ENABLE(ASSERT)
    mutable bool m_checkedCachedDrawing;
    size_t m_displayItemPosition;
#endif
#ifndef NDEBUG
    WTF::String m_clientDebugString;
#endif
};

} // namespace blink

#endif // DrawingRecorder_h
