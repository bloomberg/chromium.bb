// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintLayerPainter_h
#define PaintLayerPainter_h

#include "core/CoreExport.h"
#include "core/paint/PaintLayerFragment.h"
#include "core/paint/PaintLayerPaintingInfo.h"
#include "core/paint/PaintResult.h"
#include "wtf/Allocator.h"

namespace blink {

class ClipRect;
class PaintLayer;
class GraphicsContext;
class LayoutPoint;

// This class is responsible for painting self-painting PaintLayer.
//
// See PainterLayer SELF-PAINTING LAYER section about what 'self-painting'
// means and how it impacts this class.
class CORE_EXPORT PaintLayerPainter {
  STACK_ALLOCATED();

 public:
  enum FragmentPolicy { AllowMultipleFragments, ForceSingleFragment };

  PaintLayerPainter(PaintLayer& paintLayer) : m_paintLayer(paintLayer) {}

  // The paint() method paints the layers that intersect the damage rect from
  // back to front.  paint() assumes that the caller will clip to the bounds of
  // damageRect if necessary.
  void paint(GraphicsContext&,
             const LayoutRect& damageRect,
             const GlobalPaintFlags = GlobalPaintNormalPhase,
             PaintLayerFlags = 0);
  // paint() assumes that the caller will clip to the bounds of the painting
  // dirty if necessary.
  PaintResult paint(GraphicsContext&,
                    const PaintLayerPaintingInfo&,
                    PaintLayerFlags);
  // paintLayerContents() assumes that the caller will clip to the bounds of the
  // painting dirty rect if necessary.
  PaintResult paintLayerContents(GraphicsContext&,
                                 const PaintLayerPaintingInfo&,
                                 PaintLayerFlags,
                                 FragmentPolicy = AllowMultipleFragments);

  void paintOverlayScrollbars(GraphicsContext&,
                              const LayoutRect& damageRect,
                              const GlobalPaintFlags);

 private:
  enum ClipState { HasNotClipped, HasClipped };

  inline bool isFixedPositionObjectInPagedMedia();

  // "For paged media, boxes with fixed positions are repeated on every page."
  // https://www.w3.org/TR/2011/REC-CSS2-20110607/visuren.html#fixed-positioning
  // Repeats singleFragmentIgnoredPagination of the fixed-position object in
  // each page, with paginationOffset and layerBounds adjusted for each page.
  // TODO(wangxianzhu): Fold this into PaintLayer::collectFragments().
  void repeatFixedPositionObjectInPages(
      const PaintLayerFragment& singleFragmentIgnoredPagination,
      const PaintLayerPaintingInfo&,
      PaintLayerFragments&);

  PaintResult paintLayerContentsCompositingAllPhases(
      GraphicsContext&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      FragmentPolicy = AllowMultipleFragments);
  PaintResult paintLayerWithTransform(GraphicsContext&,
                                      const PaintLayerPaintingInfo&,
                                      PaintLayerFlags);
  PaintResult paintFragmentByApplyingTransform(
      GraphicsContext&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      const LayoutPoint& fragmentTranslation);

  PaintResult paintChildren(unsigned childrenToVisit,
                            GraphicsContext&,
                            const PaintLayerPaintingInfo&,
                            PaintLayerFlags);
  bool atLeastOneFragmentIntersectsDamageRect(
      PaintLayerFragments&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      const LayoutPoint& offsetFromRoot);
  void paintFragmentWithPhase(PaintPhase,
                              const PaintLayerFragment&,
                              GraphicsContext&,
                              const ClipRect&,
                              const PaintLayerPaintingInfo&,
                              PaintLayerFlags,
                              ClipState);
  void paintBackgroundForFragments(const PaintLayerFragments&,
                                   GraphicsContext&,
                                   const LayoutRect& transparencyPaintDirtyRect,
                                   const PaintLayerPaintingInfo&,
                                   PaintLayerFlags);
  void paintForegroundForFragments(const PaintLayerFragments&,
                                   GraphicsContext&,
                                   const LayoutRect& transparencyPaintDirtyRect,
                                   const PaintLayerPaintingInfo&,
                                   bool selectionOnly,
                                   PaintLayerFlags);
  void paintForegroundForFragmentsWithPhase(PaintPhase,
                                            const PaintLayerFragments&,
                                            GraphicsContext&,
                                            const PaintLayerPaintingInfo&,
                                            PaintLayerFlags,
                                            ClipState);
  void paintSelfOutlineForFragments(const PaintLayerFragments&,
                                    GraphicsContext&,
                                    const PaintLayerPaintingInfo&,
                                    PaintLayerFlags);
  void paintOverflowControlsForFragments(const PaintLayerFragments&,
                                         GraphicsContext&,
                                         const PaintLayerPaintingInfo&,
                                         PaintLayerFlags);
  void paintMaskForFragments(const PaintLayerFragments&,
                             GraphicsContext&,
                             const PaintLayerPaintingInfo&,
                             PaintLayerFlags);
  void paintChildClippingMaskForFragments(const PaintLayerFragments&,
                                          GraphicsContext&,
                                          const PaintLayerPaintingInfo&,
                                          PaintLayerFlags);

  void fillMaskingFragment(GraphicsContext&, const ClipRect&);

  static bool needsToClip(const PaintLayerPaintingInfo& localPaintingInfo,
                          const ClipRect&);

  // Returns whether this layer should be painted during sofware painting (i.e.,
  // not via calls from CompositedLayerMapping to draw into composited layers).
  bool shouldPaintLayerInSoftwareMode(const GlobalPaintFlags,
                                      PaintLayerFlags paintFlags);

  // Returns true if the painted output of this PaintLayer and its children is
  // invisible and therefore can't impact painted output.
  bool paintedOutputInvisible(const PaintLayerPaintingInfo&);

  PaintLayer& m_paintLayer;

  FRIEND_TEST_ALL_PREFIXES(PaintLayerPainterTest, DontPaintWithTinyOpacity);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerPainterTest,
                           DontPaintWithTinyOpacityAndBackdropFilter);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerPainterTest,
                           DoPaintWithCompositedTinyOpacity);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerPainterTest, DoPaintWithNonTinyOpacity);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerPainterTest,
                           DoPaintWithEffectAnimationZeroOpacity);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerPainterTest,
                           DoPaintWithTransformAnimationZeroOpacity);
};

}  // namespace blink

#endif  // PaintLayerPainter_h
