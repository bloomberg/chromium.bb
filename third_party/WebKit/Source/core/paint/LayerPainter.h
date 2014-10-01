// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerPainter_h
#define LayerPainter_h

#include "core/rendering/LayerFragment.h"
#include "core/rendering/LayerPaintingInfo.h"

namespace blink {

enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };

class ClipRect;
struct PaintInfo;
class LayoutPoint;
class RenderLayer;

class LayerPainter {
public:
    LayerPainter(RenderLayer& renderLayer) : m_renderLayer(renderLayer) { }

    // The paint() method paints the layers that intersect the damage rect from back to front.
    //  paint() assumes that the caller will clip to the bounds of damageRect if necessary.
    void paint(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior = PaintBehaviorNormal, RenderObject* paintingRoot = 0, PaintLayerFlags = 0);
    // paintLayer() assumes that the caller will clip to the bounds of the painting dirty if necessary.
    void paintLayer(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    // paintLayerContents() assumes that the caller will clip to the bounds of the painting dirty rect if necessary.
    void paintLayerContents(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);

    void paintOverlayScrollbars(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior, RenderObject* paintingRoot = 0);

private:

    void paintLayerContentsAndReflection(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintLayerByApplyingTransform(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& translationOffset = LayoutPoint());

    void paintChildren(unsigned childrenToVisit, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintPaginatedChildLayer(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintChildLayerIntoColumns(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const Vector<RenderLayer*>& columnLayers, size_t columnIndex);
    void updatePaintingInfoForFragments(LayerFragments&, const LayerPaintingInfo&, PaintLayerFlags, bool shouldPaintContent, const LayoutPoint* offsetFromRoot);
    void paintBackgroundForFragments(const LayerFragments&, GraphicsContext*, GraphicsContext* transparencyLayerContext,
        const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintForegroundForFragments(const LayerFragments&, GraphicsContext*, GraphicsContext* transparencyLayerContext,
        const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer,
        bool selectionOnly, PaintLayerFlags);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintOutlineForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintOverflowControlsForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintChildClippingMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintTransformedLayerIntoFragments(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void beginTransparencyLayers(GraphicsContext*, const RenderLayer* rootLayer, const LayoutRect& paintDirtyRect, const LayoutSize& subPixelAccumulation, PaintBehavior);

    void clipToRect(const LayerPaintingInfo&, GraphicsContext*, const ClipRect&, PaintLayerFlags, BorderRadiusClippingRule = IncludeSelfForBorderRadius);
    void restoreClip(GraphicsContext*, const LayoutRect& paintDirtyRect, const ClipRect&);

    // Returns whether this layer should be painted during sofware painting (i.e., not via calls from CompositedLayerMapping to draw into composited
    // layers).
    bool shouldPaintLayerInSoftwareMode(const LayerPaintingInfo&, PaintLayerFlags paintFlags);

private:
    RenderLayer& m_renderLayer;
};

} // namespace blink

#endif // LayerPainter_h
