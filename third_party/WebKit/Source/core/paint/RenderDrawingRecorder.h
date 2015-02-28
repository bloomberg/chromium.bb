// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderDrawingRecorder_h
#define RenderDrawingRecorder_h

#include "core/layout/PaintPhase.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

class GraphicsContext;
class LayoutObject;

class RenderDrawingRecorder {
public:
    RenderDrawingRecorder(GraphicsContext*, const LayoutObject&, PaintPhase, const FloatRect&);
    RenderDrawingRecorder(GraphicsContext*, const LayoutObject&, DisplayItem::Type, const FloatRect&);
    // paintRect will be pixel-snapped.
    RenderDrawingRecorder(GraphicsContext*, const LayoutObject&, DisplayItem::Type, const LayoutRect& paintRect);

    ~RenderDrawingRecorder();

    bool canUseCachedDrawing() const { return m_drawingRecorder.canUseCachedDrawing(); }

private:
    DrawingRecorder m_drawingRecorder;
#ifndef NDEBUG
    const LayoutObject& m_renderer;
#endif
};

} // namespace blink

#endif // RenderDrawingRecorder_h
