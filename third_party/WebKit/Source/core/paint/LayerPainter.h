// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerPainter_h
#define LayerPainter_h

#include "core/paint/ClipRecorder.h"
#include "core/rendering/LayerFragment.h"
#include "core/rendering/LayerPaintingInfo.h"

namespace blink {

class ClipRect;
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

    enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };

    // Set rounded clip rectangles defined by border radii all the way from the LayerPaintingInfo
    // "root" layer down to the specified layer (or the parent of said layer, in case
    // BorderRadiusClippingRule says to skip self). fragmentOffset is used for multicol, to specify
    // the translation required to get from flow thread coordinates to visual coordinates for a
    // certain column.
    // FIXME: The BorderRadiusClippingRule parameter is really useless now. If we want to skip self,
    // why not just supply the parent layer as the first parameter instead?
    static void applyRoundedRectClips(RenderLayer&, const LayerPaintingInfo&, GraphicsContext*, const LayoutPoint& fragmentOffset, PaintLayerFlags, ClipRecorder&, BorderRadiusClippingRule = IncludeSelfForBorderRadius);

private:
    enum ClipState { HasNotClipped, HasClipped };

    void paintLayerContentsAndReflection(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintLayerByApplyingTransform(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& translationOffset = LayoutPoint());

    void paintChildren(unsigned childrenToVisit, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintPaginatedChildLayer(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintChildLayerIntoColumns(RenderLayer* childLayer, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const Vector<RenderLayer*>& columnLayers, size_t columnIndex);
    bool atLeastOneFragmentIntersectsDamageRect(LayerFragments&, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& offsetFromRoot);
    void paintFragmentWithPhase(PaintPhase, const LayerFragment&, GraphicsContext*, const ClipRect&, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags, ClipState);
    void paintBackgroundForFragments(const LayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintForegroundForFragments(const LayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer,
        bool selectionOnly, PaintLayerFlags);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags, ClipState);
    void paintOutlineForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintOverflowControlsForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintChildClippingMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, RenderObject* paintingRootForRenderer, PaintLayerFlags);
    void paintTransformedLayerIntoFragments(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);

    static bool needsToClip(const LayerPaintingInfo& localPaintingInfo, const ClipRect&);

    // Returns whether this layer should be painted during sofware painting (i.e., not via calls from CompositedLayerMapping to draw into composited
    // layers).
    bool shouldPaintLayerInSoftwareMode(const LayerPaintingInfo&, PaintLayerFlags paintFlags);

    RenderLayer& m_renderLayer;
};

} // namespace blink

#endif // LayerPainter_h
