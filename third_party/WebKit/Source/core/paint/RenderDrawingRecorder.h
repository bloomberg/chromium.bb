// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderDrawingRecorder_h
#define RenderDrawingRecorder_h

#include "core/rendering/PaintPhase.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

class GraphicsContext;
class RenderObject;

class RenderDrawingRecorder {
public:
    explicit RenderDrawingRecorder(GraphicsContext*, const RenderObject&, PaintPhase, const FloatRect&);

    ~RenderDrawingRecorder();

    bool canUseCachedDrawing() const { return m_drawingRecorder.canUseCachedDrawing(); }

private:
    DrawingRecorder m_drawingRecorder;
#ifndef NDEBUG
    const RenderObject& m_renderer;
#endif
};

} // namespace blink

#endif // RenderDrawingRecorder_h
