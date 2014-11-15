// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilterPainter_h
#define FilterPainter_h

#include "core/paint/ViewDisplayList.h"
#include "core/rendering/LayerPaintingInfo.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ClipRecorder;
class ClipRect;
class GraphicsContext;
class RenderLayer;

class BeginFilterDisplayItem : public DisplayItem {
public:
    BeginFilterDisplayItem(const RenderObject* renderer, Type type, PassRefPtr<ImageFilter> imageFilter, const LayoutRect& bounds)
        : DisplayItem(renderer, type), m_imageFilter(imageFilter), m_bounds(bounds) { }
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif

    RefPtr<ImageFilter> m_imageFilter;
    const LayoutRect m_bounds;
};

class EndFilterDisplayItem : public DisplayItem {
public:
    EndFilterDisplayItem(const RenderObject* renderer)
        : DisplayItem(renderer, EndFilter) { }
    virtual void replay(GraphicsContext*) override;

#ifndef NDEBUG
    virtual WTF::String asDebugString() const override;
#endif
};

class FilterPainter {
public:
    FilterPainter(RenderLayer&, GraphicsContext*, const LayoutPoint& offsetFromRoot, const ClipRect&, LayerPaintingInfo&, PaintLayerFlags paintFlags, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed);
    ~FilterPainter();

private:
    bool m_filterInProgress;
    GraphicsContext* m_context;
    OwnPtr<ClipRecorder> m_clipRecorder;
    RenderObject* m_renderer;
};

} // namespace blink

#endif // FilterPainter_h
