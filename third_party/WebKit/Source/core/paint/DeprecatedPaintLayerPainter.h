// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeprecatedPaintLayerPainter_h
#define DeprecatedPaintLayerPainter_h

#include "core/paint/DeprecatedPaintLayerFragment.h"
#include "core/paint/DeprecatedPaintLayerPaintingInfo.h"

namespace blink {

class ClipRect;
class DeprecatedPaintLayer;
class LayoutPoint;

class DeprecatedPaintLayerPainter {
public:
    enum FragmentPolicy { AllowMultipleFragments, ForceSingleFragment };

    DeprecatedPaintLayerPainter(DeprecatedPaintLayer& renderLayer) : m_renderLayer(renderLayer) { }

    // The paint() method paints the layers that intersect the damage rect from back to front.
    //  paint() assumes that the caller will clip to the bounds of damageRect if necessary.
    void paint(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior = PaintBehaviorNormal, LayoutObject* paintingRoot = 0, PaintLayerFlags = 0);
    // paintLayer() assumes that the caller will clip to the bounds of the painting dirty if necessary.
    void paintLayer(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    // paintLayerContents() assumes that the caller will clip to the bounds of the painting dirty rect if necessary.
    void paintLayerContents(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);

    void paintOverlayScrollbars(GraphicsContext*, const LayoutRect& damageRect, PaintBehavior, LayoutObject* paintingRoot = 0);

private:
    enum ClipState { HasNotClipped, HasClipped };

    void paintLayerContentsAndReflection(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);
    void paintLayerWithTransform(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    void paintFragmentByApplyingTransform(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& fragmentTranslation);

    void paintChildren(unsigned childrenToVisit, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    void paintPaginatedChildLayer(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    void paintChildLayerIntoColumns(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, const Vector<DeprecatedPaintLayer*>& columnLayers, size_t columnIndex);
    bool atLeastOneFragmentIntersectsDamageRect(DeprecatedPaintLayerFragments&, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& offsetFromRoot);
    void paintFragmentWithPhase(PaintPhase, const DeprecatedPaintLayerFragment&, GraphicsContext*, const ClipRect&, const DeprecatedPaintLayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags, ClipState);
    void paintBackgroundForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const DeprecatedPaintLayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags);
    void paintForegroundForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const DeprecatedPaintLayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer,
        bool selectionOnly, PaintLayerFlags);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags, ClipState);
    void paintOutlineForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintBehavior, LayoutObject* paintingRootForRenderer, PaintLayerFlags);
    void paintOverflowControlsForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    void paintMaskForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForRenderer, PaintLayerFlags);
    void paintChildClippingMaskForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForRenderer, PaintLayerFlags);

    static bool needsToClip(const DeprecatedPaintLayerPaintingInfo& localPaintingInfo, const ClipRect&);

    // Returns whether this layer should be painted during sofware painting (i.e., not via calls from CompositedDeprecatedPaintLayerMapping to draw into composited
    // layers).
    bool shouldPaintLayerInSoftwareMode(const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags paintFlags);

    DeprecatedPaintLayer& m_renderLayer;
};

} // namespace blink

#endif // DeprecatedPaintLayerPainter_h
