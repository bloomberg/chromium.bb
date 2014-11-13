// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingRecorder_h
#define DrawingRecorder_h

#include "core/rendering/PaintPhase.h"
#include "platform/geometry/FloatRect.h"

namespace blink {

class GraphicsContext;
class RenderObject;

class DrawingRecorder {
public:
    explicit DrawingRecorder(GraphicsContext*, const RenderObject*, PaintPhase, const FloatRect&);
    ~DrawingRecorder();

private:
    GraphicsContext* m_context;
    const RenderObject* m_renderer;
    const PaintPhase m_phase;
    const FloatRect m_bounds;
};

} // namespace blink

#endif // DrawingRecorder_h
