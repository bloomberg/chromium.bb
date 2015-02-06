// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterPainter_h
#define FilterPainter_h

#include "core/layout/LayerPaintingInfo.h"
#include "wtf/OwnPtr.h"

namespace blink {

class LayerClipRecorder;
class ClipRect;
class GraphicsContext;
class Layer;

class FilterPainter {
public:
    FilterPainter(Layer&, GraphicsContext*, const LayoutPoint& offsetFromRoot, const ClipRect&, LayerPaintingInfo&, PaintLayerFlags paintFlags, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed);
    ~FilterPainter();

private:
    bool m_filterInProgress;
    GraphicsContext* m_context;
    OwnPtr<LayerClipRecorder> m_clipRecorder;
    LayoutObject* m_renderer;
};

} // namespace blink

#endif // FilterPainter_h
