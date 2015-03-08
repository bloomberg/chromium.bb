// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutObjectDrawingRecorder_h
#define LayoutObjectDrawingRecorder_h

#include "core/layout/PaintPhase.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

class GraphicsContext;
class LayoutObject;

class LayoutObjectDrawingRecorder {
public:
    LayoutObjectDrawingRecorder(GraphicsContext*, const LayoutObject&, PaintPhase, const FloatRect&);
    LayoutObjectDrawingRecorder(GraphicsContext*, const LayoutObject&, DisplayItem::Type, const FloatRect&);
    // paintRect will be pixel-snapped.
    LayoutObjectDrawingRecorder(GraphicsContext*, const LayoutObject&, DisplayItem::Type, const LayoutRect& paintRect);

    ~LayoutObjectDrawingRecorder();

    bool canUseCachedDrawing() const { return m_drawingRecorder.canUseCachedDrawing(); }

private:
    DrawingRecorder m_drawingRecorder;
#ifndef NDEBUG
    const LayoutObject& m_layoutObject;
#endif
};

} // namespace blink

#endif // LayoutObjectDrawingRecorder_h
