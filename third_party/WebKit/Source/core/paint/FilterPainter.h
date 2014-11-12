// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterPainter_h
#define FilterPainter_h

#include "core/rendering/LayerPaintingInfo.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ClipRecorder;
class ClipRect;
class FloatRect;
class GraphicsContext;
class RenderLayer;

class FilterPainter {
public:
    FilterPainter(RenderLayer&, GraphicsContext*, const FloatRect& filterBoxRect, const ClipRect&, const LayerPaintingInfo&, PaintLayerFlags paintFlags);
    ~FilterPainter();

private:
    bool m_filterInProgress;
    GraphicsContext* m_context;
    OwnPtr<ClipRecorder> m_clipRecorder;
};

} // namespace blink

#endif // FilterPainter_h
