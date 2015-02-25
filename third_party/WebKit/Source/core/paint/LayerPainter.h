// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayerPainter_h
#define LayerPainter_h

#include "core/layout/LayerFragment.h"
#include "core/layout/LayerPaintingInfo.h"

namespace blink {

class ClipRect;
class LayoutPoint;
class Layer;

class LayerPainter {
public:
    enum FragmentPolicy { AllowMultipleFragments, ForceSingleFragment };

    LayerPainter(Layer& renderLayer) : m_renderLayer(renderLayer) { }

    // The paint() method paints the layers that intersect the damage rect from back to front.
    //  paint() assumes that the caller will clip to the bounds of damageRect if necessary.
    void paint(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior = PaintBehaviorNormal, LayoutObject* paintingRoot = 0, PaintLayerFlags = 0);
    // paintLayer() assumes that the caller will clip to the bounds of the painting dirty if necessary.
    void paintLayer(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    // paintLayerContents() assumes that the caller will clip to the bounds of the painting dirty rect if necessary.
    void paintLayerContents(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);

    void paintOverlayScrollbars(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior, LayoutObject* paintingRoot = 0);

private:
    enum ClipState { HasNotClipped, HasClipped };

    void paintLayerContentsAndReflection(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);
    void paintLayerWithTransform(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintFragmentByApplyingTransform(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& fragmentTranslation);

    void paintChildren(unsigned childrenToVisit, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintPaginatedChildLayer(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintChildLayerIntoColumns(GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags, const Vector<Layer*>& columnLayers, size_t columnIndex);
    bool atLeastOneFragmentIntersectsDamageRect(LayerFragments&, const LayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& offsetFromRoot);
    void paintFragmentWithPhase(PaintPhase, const LayerFragment&, GraphicsContext*, const ClipRect&, const LayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags, ClipState);
    void paintBackgroundForFragments(const LayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const LayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags);
    void paintForegroundForFragments(const LayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const LayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer,
        bool selectionOnly, PaintLayerFlags);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags, ClipState);
    void paintOutlineForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags);
    void paintOverflowControlsForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, PaintLayerFlags);
    void paintMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, LayoutObject* paintingRootForRenderer, PaintLayerFlags);
    void paintChildClippingMaskForFragments(const LayerFragments&, GraphicsContext*, const LayerPaintingInfo&, LayoutObject* paintingRootForRenderer, PaintLayerFlags);

    static bool needsToClip(const LayerPaintingInfo& localPaintingInfo, const ClipRect&);

    // Returns whether this layer should be painted during sofware painting (i.e., not via calls from CompositedLayerMapping to draw into composited
    // layers).
    bool shouldPaintLayerInSoftwareMode(const LayerPaintingInfo&, PaintLayerFlags paintFlags);

    Layer& m_renderLayer;
};

} // namespace blink

#endif // LayerPainter_h
