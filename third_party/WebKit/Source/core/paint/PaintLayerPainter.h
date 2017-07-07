// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintLayerPainter_h
#define PaintLayerPainter_h

#include "core/CoreExport.h"
#include "core/paint/PaintLayerFragment.h"
#include "core/paint/PaintLayerPaintingInfo.h"
#include "core/paint/PaintResult.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ClipRect;
class DisplayItemClient;
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
  enum FragmentPolicy { kAllowMultipleFragments, kForceSingleFragment };

  PaintLayerPainter(PaintLayer& paint_layer) : paint_layer_(paint_layer) {}

  // The paint() method paints the layers that intersect the damage rect from
  // back to front.  paint() assumes that the caller will clip to the bounds of
  // damageRect if necessary.
  void Paint(GraphicsContext&,
             const LayoutRect& damage_rect,
             const GlobalPaintFlags = kGlobalPaintNormalPhase,
             PaintLayerFlags = 0);
  // paint() assumes that the caller will clip to the bounds of the painting
  // dirty if necessary.
  PaintResult Paint(GraphicsContext&,
                    const PaintLayerPaintingInfo&,
                    PaintLayerFlags);
  // paintLayerContents() assumes that the caller will clip to the bounds of the
  // painting dirty rect if necessary.
  PaintResult PaintLayerContents(GraphicsContext&,
                                 const PaintLayerPaintingInfo&,
                                 PaintLayerFlags,
                                 FragmentPolicy = kAllowMultipleFragments);

  void PaintOverlayScrollbars(GraphicsContext&,
                              const LayoutRect& damage_rect,
                              const GlobalPaintFlags);

 private:
  friend class PaintLayerPainterTest;

  enum ClipState { kHasNotClipped, kHasClipped };

  inline bool IsFixedPositionObjectInPagedMedia();

  // "For paged media, boxes with fixed positions are repeated on every page."
  // https://www.w3.org/TR/2011/REC-CSS2-20110607/visuren.html#fixed-positioning
  // Repeats singleFragmentIgnoredPagination of the fixed-position object in
  // each page, with paginationOffset and layerBounds adjusted for each page.
  // TODO(wangxianzhu): Fold this into PaintLayer::collectFragments().
  void RepeatFixedPositionObjectInPages(
      const PaintLayerFragment& single_fragment_ignored_pagination,
      const PaintLayerPaintingInfo&,
      PaintLayerFragments&);

  PaintResult PaintLayerContentsCompositingAllPhases(
      GraphicsContext&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      FragmentPolicy = kAllowMultipleFragments);
  PaintResult PaintLayerWithTransform(GraphicsContext&,
                                      const PaintLayerPaintingInfo&,
                                      PaintLayerFlags);
  PaintResult PaintFragmentByApplyingTransform(
      GraphicsContext&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      const LayoutPoint& fragment_translation);

  PaintResult PaintChildren(unsigned children_to_visit,
                            GraphicsContext&,
                            const PaintLayerPaintingInfo&,
                            PaintLayerFlags);
  bool AtLeastOneFragmentIntersectsDamageRect(
      PaintLayerFragments&,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags,
      const LayoutPoint& offset_from_root);
  void PaintFragmentWithPhase(PaintPhase,
                              const PaintLayerFragment&,
                              GraphicsContext&,
                              const ClipRect&,
                              const PaintLayerPaintingInfo&,
                              PaintLayerFlags,
                              ClipState);
  void PaintBackgroundForFragments(
      const PaintLayerFragments&,
      GraphicsContext&,
      const LayoutRect& transparency_paint_dirty_rect,
      const PaintLayerPaintingInfo&,
      PaintLayerFlags);
  void PaintForegroundForFragments(
      const PaintLayerFragments&,
      GraphicsContext&,
      const LayoutRect& transparency_paint_dirty_rect,
      const PaintLayerPaintingInfo&,
      bool selection_only,
      PaintLayerFlags);
  void PaintForegroundForFragmentsWithPhase(PaintPhase,
                                            const PaintLayerFragments&,
                                            GraphicsContext&,
                                            const PaintLayerPaintingInfo&,
                                            PaintLayerFlags,
                                            ClipState);
  void PaintSelfOutlineForFragments(const PaintLayerFragments&,
                                    GraphicsContext&,
                                    const PaintLayerPaintingInfo&,
                                    PaintLayerFlags);
  void PaintOverflowControlsForFragments(const PaintLayerFragments&,
                                         GraphicsContext&,
                                         const PaintLayerPaintingInfo&,
                                         PaintLayerFlags);
  void PaintMaskForFragments(const PaintLayerFragments&,
                             GraphicsContext&,
                             const PaintLayerPaintingInfo&,
                             PaintLayerFlags);
  void PaintChildClippingMaskForFragments(const PaintLayerFragments&,
                                          GraphicsContext&,
                                          const PaintLayerPaintingInfo&,
                                          PaintLayerFlags);

  void FillMaskingFragment(GraphicsContext&,
                           const ClipRect&,
                           const DisplayItemClient&);

  static bool NeedsToClip(const PaintLayerPaintingInfo& local_painting_info,
                          const ClipRect&,
                          const PaintLayerFlags&);

  // Returns whether this layer should be painted during sofware painting (i.e.,
  // not via calls from CompositedLayerMapping to draw into composited layers).
  bool ShouldPaintLayerInSoftwareMode(const GlobalPaintFlags,
                                      PaintLayerFlags paint_flags);

  // Returns true if the painted output of this PaintLayer and its children is
  // invisible and therefore can't impact painted output.
  bool PaintedOutputInvisible(const PaintLayerPaintingInfo&);

  PaintLayer& paint_layer_;
};

}  // namespace blink

#endif  // PaintLayerPainter_h
