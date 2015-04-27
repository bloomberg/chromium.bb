// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutObjectDrawingRecorder_h
#define LayoutObjectDrawingRecorder_h

#include "core/paint/PaintPhase.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

class GraphicsContext;
class LayoutObject;

// Convienance constructors for creating DrawingRecorders.
class LayoutObjectDrawingRecorder final : public DrawingRecorder {
public:
    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const LayoutRect& clip)
        : DrawingRecorder(context, layoutObject, displayItemType, pixelSnappedIntRect(clip)) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, PaintPhase phase, const FloatRect& clip)
        : DrawingRecorder(context, layoutObject, DisplayItem::paintPhaseToDrawingType(phase), clip) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type type, const FloatRect& clip)
        : DrawingRecorder(context, layoutObject, type, clip) { }
};

} // namespace blink

#endif // LayoutObjectDrawingRecorder_h
