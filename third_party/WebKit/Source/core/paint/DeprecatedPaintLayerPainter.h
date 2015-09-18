// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeprecatedPaintLayerPainter_h
#define DeprecatedPaintLayerPainter_h

#include "core/CoreExport.h"
#include "core/paint/DeprecatedPaintLayerFragment.h"
#include "core/paint/DeprecatedPaintLayerPaintingInfo.h"
#include "wtf/Allocator.h"

namespace blink {

class ClipRect;
class DeprecatedPaintLayer;
class GraphicsContext;
class LayoutPoint;

class CORE_EXPORT DeprecatedPaintLayerPainter {
    STACK_ALLOCATED();
public:
    enum FragmentPolicy { AllowMultipleFragments, ForceSingleFragment };

    enum PaintResult {
        // The layer is fully painted. This includes cases that nothing needs painting
        // regardless of the paint rect.
        FullyPainted,
        // Some part of the layer is out of the paint rect and may be not fully painted.
        // The results cannot be cached because they may change when paint rect changes.
        MaybeNotFullyPainted
    };

    DeprecatedPaintLayerPainter(DeprecatedPaintLayer& paintLayer) : m_paintLayer(paintLayer) { }

    // The paint() method paints the layers that intersect the damage rect from back to front.
    //  paint() assumes that the caller will clip to the bounds of damageRect if necessary.
    void paint(GraphicsContext*, const LayoutRect& damageRect, const GlobalPaintFlags = GlobalPaintNormalPhase, LayoutObject* paintingRoot = 0, PaintLayerFlags = 0);
    // paintLayer() assumes that the caller will clip to the bounds of the painting dirty if necessary.
    PaintResult paintLayer(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    // paintLayerContents() assumes that the caller will clip to the bounds of the painting dirty rect if necessary.
    PaintResult paintLayerContents(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);

    void paintOverlayScrollbars(GraphicsContext*, const LayoutRect& damageRect, const GlobalPaintFlags, LayoutObject* paintingRoot = 0);

private:
    enum ClipState { HasNotClipped, HasClipped };

    PaintResult paintLayerInternal(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    PaintResult paintLayerContentsAndReflection(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, FragmentPolicy = AllowMultipleFragments);
    PaintResult paintLayerWithTransform(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    PaintResult paintFragmentByApplyingTransform(GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& fragmentTranslation);

    PaintResult paintChildren(unsigned childrenToVisit, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    bool atLeastOneFragmentIntersectsDamageRect(DeprecatedPaintLayerFragments&, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags, const LayoutPoint& offsetFromRoot);
    void paintFragmentWithPhase(PaintPhase, const DeprecatedPaintLayerFragment&, GraphicsContext*, const ClipRect&, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject, PaintLayerFlags, ClipState);
    void paintBackgroundForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject, PaintLayerFlags);
    void paintForegroundForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*,
        const LayoutRect& transparencyPaintDirtyRect, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject,
        bool selectionOnly, PaintLayerFlags);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject, PaintLayerFlags, ClipState);
    void paintOutlineForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject, PaintLayerFlags);
    void paintOverflowControlsForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, PaintLayerFlags);
    void paintMaskForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject, PaintLayerFlags);
    void paintChildClippingMaskForFragments(const DeprecatedPaintLayerFragments&, GraphicsContext*, const DeprecatedPaintLayerPaintingInfo&, LayoutObject* paintingRootForLayoutObject, PaintLayerFlags);

    static bool needsToClip(const DeprecatedPaintLayerPaintingInfo& localPaintingInfo, const ClipRect&);

    // Returns whether this layer should be painted during sofware painting (i.e., not via calls from CompositedDeprecatedPaintLayerMapping to draw into composited
    // layers).
    bool shouldPaintLayerInSoftwareMode(const GlobalPaintFlags, PaintLayerFlags paintFlags);

    DeprecatedPaintLayer& m_paintLayer;
};

} // namespace blink

#endif // DeprecatedPaintLayerPainter_h
