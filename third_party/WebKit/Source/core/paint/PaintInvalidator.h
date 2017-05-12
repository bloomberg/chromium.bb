// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidator_h
#define PaintInvalidator_h

#include "core/layout/LayoutObject.h"
#include "core/paint/PaintPropertyTreeBuilder.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Vector.h"

namespace blink {

struct PaintInvalidatorContext {
  USING_FAST_MALLOC(PaintInvalidatorContext);

 public:
  PaintInvalidatorContext() : parent_context(nullptr) {}

  PaintInvalidatorContext(const PaintInvalidatorContext& parent_context)
      : parent_context(&parent_context),
        forced_subtree_invalidation_flags(
            parent_context.forced_subtree_invalidation_flags),
        paint_invalidation_container(
            parent_context.paint_invalidation_container),
        paint_invalidation_container_for_stacked_contents(
            parent_context.paint_invalidation_container_for_stacked_contents),
        painting_layer(parent_context.painting_layer) {}

  // This method is virtual temporarily to adapt PaintInvalidatorContext and the
  // legacy PaintInvalidationState for code shared by old code and new code.
  virtual void MapLocalRectToVisualRectInBacking(const LayoutObject&,
                                                 LayoutRect&) const;

  bool NeedsVisualRectUpdate(const LayoutObject& object) const {
    if (!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
      return true;
#if DCHECK_IS_ON()
    if (force_visual_rect_update_for_checking_)
      return true;
#endif
    return object.NeedsPaintOffsetAndVisualRectUpdate() ||
           (forced_subtree_invalidation_flags &
            PaintInvalidatorContext::kForcedSubtreeVisualRectUpdate);
  }

  const PaintInvalidatorContext* parent_context;

  enum ForcedSubtreeInvalidationFlag {
    kForcedSubtreeInvalidationChecking = 1 << 0,
    kForcedSubtreeVisualRectUpdate = 1 << 1,
    kForcedSubtreeFullInvalidation = 1 << 2,
    kForcedSubtreeFullInvalidationForStackedContents = 1 << 3,
    kForcedSubtreeSVGResourceChange = 1 << 4,

    // TODO(crbug.com/637313): This is temporary before we support filters in
    // paint property tree.
    kForcedSubtreeSlowPathRect = 1 << 5,

    // The paint invalidation tree walk invalidates paint caches, such as
    // DisplayItemClients and subsequence caches, and also the regions
    // into which objects raster pixels. When this flag is set, raster region
    // invalidations are not issued.
    //
    // Context: some objects in this paint walk, for example SVG resource
    // container subtress, don't actually have any raster regions, because they
    // are used as "painting subroutines" for one or more other locations in
    // SVG.
    kForcedSubtreeNoRasterInvalidation = 1 << 6,
  };
  unsigned forced_subtree_invalidation_flags = 0;

  // The following fields can be null only before
  // PaintInvalidator::updateContext().

  // The current paint invalidation container for normal flow objects.
  // It is the enclosing composited object.
  const LayoutBoxModelObject* paint_invalidation_container = nullptr;

  // The current paint invalidation container for stacked contents (stacking
  // contexts or positioned objects).  It is the nearest ancestor composited
  // object which establishes a stacking context.  See
  // Source/core/paint/README.md ### PaintInvalidationState for details on how
  // stacked contents' paint invalidation containers differ.
  const LayoutBoxModelObject*
      paint_invalidation_container_for_stacked_contents = nullptr;

  PaintLayer* painting_layer = nullptr;

  // Store the old visual rect in the paint invalidation backing's coordinates.
  // It does *not* account for composited scrolling.
  // See LayoutObject::adjustVisualRectForCompositedScrolling().
  LayoutRect old_visual_rect;
  // Use LayoutObject::visualRect() to get the new visual rect.

  // Store the origin of the object's local coordinates in the paint
  // invalidation backing's coordinates. They are used to detect layoutObject
  // shifts that force a full invalidation and invalidation check in subtree.
  // The points do *not* account for composited scrolling. See
  // LayoutObject::adjustVisualRectForCompositedScrolling().
  LayoutPoint old_location;
  LayoutPoint new_location;

 private:
  friend class PaintInvalidator;
  const PaintPropertyTreeBuilderFragmentContext* tree_builder_context_ =
      nullptr;

#if DCHECK_IS_ON()
  bool tree_builder_context_actually_needed_ = false;
  friend class FindVisualRectNeedingUpdateScope;
  friend class FindVisualRectNeedingUpdateScopeBase;
  mutable bool force_visual_rect_update_for_checking_ = false;
#endif
};

class PaintInvalidator {
 public:
  void InvalidatePaint(FrameView&,
                       const PaintPropertyTreeBuilderContext*,
                       PaintInvalidatorContext&);
  void InvalidatePaint(const LayoutObject&,
                       const PaintPropertyTreeBuilderContext*,
                       PaintInvalidatorContext&);

  // Process objects needing paint invalidation on the next frame.
  // See the definition of PaintInvalidationDelayedFull for more details.
  void ProcessPendingDelayedPaintInvalidations();

 private:
  friend struct PaintInvalidatorContext;
  template <typename Rect, typename Point>
  static LayoutRect MapLocalRectToVisualRectInBacking(
      const LayoutObject&,
      const Rect&,
      const PaintInvalidatorContext&);

  ALWAYS_INLINE LayoutRect
  ComputeVisualRectInBacking(const LayoutObject&,
                             const PaintInvalidatorContext&);
  ALWAYS_INLINE LayoutPoint
  ComputeLocationInBacking(const LayoutObject&, const PaintInvalidatorContext&);
  ALWAYS_INLINE void UpdatePaintingLayer(const LayoutObject&,
                                         PaintInvalidatorContext&);
  ALWAYS_INLINE void UpdatePaintInvalidationContainer(const LayoutObject&,
                                                      PaintInvalidatorContext&);
  ALWAYS_INLINE void UpdateVisualRectIfNeeded(
      const LayoutObject&,
      const PaintPropertyTreeBuilderContext*,
      PaintInvalidatorContext&);
  ALWAYS_INLINE void UpdateVisualRect(const LayoutObject&,
                                      PaintInvalidatorContext&);

  Vector<const LayoutObject*> pending_delayed_paint_invalidations_;
};

}  // namespace blink

#endif  // PaintInvalidator_h
