// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutObjectDrawingRecorder_h
#define LayoutObjectDrawingRecorder_h

#include "core/layout/PaintPhase.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

class GraphicsContext;
class LayoutObject;

class LayoutObjectDrawingRecorder {
public:
    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const LayoutRect& clip)
        : m_drawingRecorder(context, layoutObject, displayItemType, pixelSnappedIntRect(clip)) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, PaintPhase phase, const FloatRect& clip)
        : m_drawingRecorder(context, layoutObject, DisplayItem::paintPhaseToDrawingType(phase), clip) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type type, const FloatRect& clip)
        : m_drawingRecorder(context, layoutObject, type, clip) { }

    bool canUseCachedDrawing() const { return m_drawingRecorder.canUseCachedDrawing(); }

private:
    DrawingRecorder m_drawingRecorder;
};

} // namespace blink

#endif // LayoutObjectDrawingRecorder_h
